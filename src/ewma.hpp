#ifndef CXXMETRICS_EWMA_HPP
#define CXXMETRICS_EWMA_HPP

#include "metric.hpp"
#include <cmath>
#include <chrono>
#include <atomic>

namespace cxxmetrics
{

namespace internal
{

template<typename T>
struct atomic_adder
{
    void operator()(std::atomic<T>& a, const T& b) const
    {
        a += b;
    }
};

template<typename T>
struct manual_atomic_adder
{
    void operator()(std::atomic<T>& a, const T& b) const
    {
        while (true) {
            T v1 = a.load();
            T v2 = v1 + b;

            if (a.compare_exchange_weak(v1, v2))
                break;
        }

    }
};

template<> struct atomic_adder<float> : public manual_atomic_adder<float> {};
template<> struct atomic_adder<double> : public manual_atomic_adder<double> {};
template<> struct atomic_adder<long double> : public manual_atomic_adder<long double> {};

template<typename TA, typename TB>
inline void atomic_add(std::atomic<TA>& a, const TB& b)
{
    atomic_adder<TA> add;
    add(a, b);
}

template<typename TClockGet, typename TValue = double>
class ewma
{
    static_assert(std::is_arithmetic<TValue>::value, "ewma can only be applied to integral and floating point values");
    static auto clk_point_()
    {
        TClockGet *clk;
        return (*clk)();
    }

    static auto clk_diff_()
    {
        return clk_point_() - clk_point_();
    }

public:
    using clock_point = typename std::decay<decltype(ewma<TClockGet, TValue>::clk_point_())>::type;
    using clock_diff = typename std::decay<decltype(ewma<TClockGet, TValue>::clk_diff_())>::type;
private:
    TClockGet clk_;
    double alpha_;
    clock_diff interval_;
    clock_diff window_;
    std::atomic<TValue> rate_;
    clock_point last_;
    std::atomic<TValue> pending_;

    static double get_alpha(const clock_diff &interval, const clock_diff &window)
    {
        return 1 - exp((interval * -1.0) / window);
    }

    void tick(const clock_point &at) noexcept;

public:
    ewma(const clock_diff &window, const clock_diff &interval, const TClockGet &clock = TClockGet()) noexcept;
    ewma(const ewma &e) noexcept;
    ~ewma() = default;

    template<typename TAmt>
    void mark(TAmt amount) noexcept;

    bool compare_exchange(TValue &expectedrate, TValue rate) noexcept;

    TValue rate() noexcept;

    TValue rate() const noexcept;

    ewma &operator=(const ewma<TClockGet, TValue> &c) noexcept;

};

template<typename TClockGet, typename TValue>
ewma<TClockGet, TValue>::ewma(const clock_diff &window, const clock_diff &interval, const TClockGet &clock) noexcept :
        clk_(clock),
        alpha_(get_alpha(interval, window)),
        interval_(interval),
        window_(window),
        rate_(-1.0),
        pending_(0)
{
    last_ = clk_();
}

template<typename TClockGet, typename TValue>
ewma<TClockGet, TValue>::ewma(const ewma<TClockGet, TValue> &c) noexcept :
    clk_(c.clk_),
    alpha_(c.alpha_),
    interval_(c.interval_),
    window_(c.window_),
    rate_(c.rate_.load()),
    last_(c.last_),
    pending_(c.pending_.load())
{

}

template<typename TClockGet, typename TValue>
template<typename TMark>
void ewma<TClockGet, TValue>::mark(TMark amount) noexcept
{
    auto now = clk_();

    // our clock went backwards
    if (now < last_)
        return;

    // See if we crossed the interval threshold. If so we need to tick
    if ((now - last_) >= interval_)
        tick(now);

    atomic_add(pending_, amount);
}

template<typename TClockGet, typename TValue>
bool ewma<TClockGet, TValue>::compare_exchange(TValue &expectedrate, TValue rate) noexcept
{
    return rate_.compare_exchange_weak(expectedrate, rate);
}

template<typename TClockGet, typename TValue>
TValue ewma<TClockGet, TValue>::rate() noexcept
{
    auto rate = rate_.load();
    if (rate < 0)
        return 0;

    auto now = clk_();
    if ((now - last_) >= interval_)
        tick(now);

    return rate;
}

template<typename TClockGet, typename TValue>
TValue ewma<TClockGet, TValue>::rate() const noexcept
{
    auto rate = rate_.load();
    if (rate < 0)
        return 0;

    return rate;
}

template<typename TClockGet, typename TValue>
void ewma<TClockGet, TValue>::tick(const clock_point &at) noexcept
{
    TValue rate;
    int missed_intervals;
    clock_point last;

    auto pending = pending_.load();

cxxmetrics_ewma_startover:
    rate = rate_.load();
    last = last_;
    if (rate < 0)
    {
        // one thread sets the last timestamp
        if (!pending_.compare_exchange_weak(pending, 0))
            goto cxxmetrics_ewma_startover;

        if (rate_.compare_exchange_weak(rate, pending))
        {
            last_ = at;
            return;
        }
    }

    // make sure that last_ didn't catch up with us
    if ((at - last) < interval_)
        return;

    // apply the pending value to our current rate
    // if someone else already snagged the pending value, start over
    rate = rate + (alpha_ * (pending - rate));

    // figure out how many intervals we've missed
    missed_intervals = ((at - last) / interval_) - 1;

    if (missed_intervals)
    {
        // we missed some intervals - we'll average in zeros
        if ((at - last) > window_)
        {
            while ((at - last) > window_)
            {
                rate = sqrt(rate);
                last += window_;
            }

            // figure out the missed intervals now
            missed_intervals = ((at - last) / interval_) - 1;
        }

        for (int i = 0; i < missed_intervals; i++)
            rate = rate + (alpha_ * -rate);
    }

    if (!pending_.compare_exchange_weak(pending, 0))
        goto cxxmetrics_ewma_startover;

    rate_.store(rate);
    last_ = at;
}

template<typename TClockGet, typename TValue>
ewma<TClockGet, TValue> &ewma<TClockGet, TValue>::operator=(const ewma<TClockGet, TValue> &c) noexcept
{
    alpha_ = c.alpha_;
    interval_ = c.interval_;
    window_ = c.window_;
    rate_.store(c.rate_.load());
    pending_.store(c.pending_.load());
    last_ = c.last_;
    return *this;
}

}

/**
 * \brief A default TClockGet implementation using the system steady_clock
 */
struct steady_clock_point
{
    std::chrono::steady_clock::time_point operator()() const noexcept
    {
        return std::chrono::steady_clock::now();
    }
};

/**
 * \brief An exponential weighted moving average metric
 */
template<typename TValue>
class ewma : public metric<ewma<TValue>>
{
    internal::ewma<steady_clock_point, TValue> ewma_;
public:
    /**
     * \brief Construct an exponential weighted moving average
     *
     * \param window The window over which the average accounts for
     * \param interval The interval at which the average is calculated
     */
    explicit ewma(std::chrono::steady_clock::duration window,
         std::chrono::steady_clock::duration interval = std::chrono::seconds(5)) noexcept :
            ewma_(window, interval)
    { }

    ewma(const ewma &ewma) noexcept = default;
    virtual ~ewma() = default;

    ewma &operator=(const ewma &e) noexcept = default;

    /**
     * \brief Mark the value in the ewma
     *
     * \param value the value to mark in the ewma
     */
    template<typename TMark>
    typename std::enable_if<std::is_arithmetic<TMark>::value, void>::type mark(TMark value) noexcept
    {
        ewma_.mark(value);
    }

    /**
     * \brief Get the current rate in the ewma
     *
     * \return the rate of the ewma
     */
    TValue rate() const noexcept
    {
        return ewma_.rate();
    }

    /**
     * \brief Get the current rate in the ewma
     *
     * \return The rate of the ewma
     */
    TValue rate() noexcept
    {
        return ewma_.rate();
    }

    /**
     * \brief Convenience operator to mark a value in the moving average
     *
     * \param value the amount to mark
     * \return a reference to the ewma
     */
    template<typename TMark>
    typename std::enable_if<std::is_arithmetic<TMark>::value, ewma<TValue>>::type& operator+=(typename std::enable_if<std::is_arithmetic<TMark>::value, TMark>::type value) noexcept
    {
        mark(value);
        return *this;
    }

protected:
    bool compare_exchange(TValue expectedrate, TValue rate)
    {
        return ewma_.compare_exchange(expectedrate, rate);
    }
};

}

#endif //CXXMETRICS_EWMA_HPP
