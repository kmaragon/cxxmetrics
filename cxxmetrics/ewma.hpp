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

template<typename TClockGet>
class clock_traits
{
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
    using clock_point = typename std::decay<decltype(clk_point_())>::type;
    using clock_diff = typename std::decay<decltype(clk_diff_())>::type;
};

template<typename TClockGet, period::value TWindow, period::value TInterval, typename TValue = double>
class ewma
{
    static_assert(std::is_arithmetic<TValue>::value, "ewma can only be applied to integral and floating point values");

public:
    using clock_point = typename clock_traits<TClockGet>::clock_point;
    using clock_diff = typename clock_traits<TClockGet>::clock_diff;

private:
    static long double alpha_;

    TClockGet clk_;
    std::atomic<TValue> rate_;
    clock_point last_;
    std::atomic<TValue> pending_;
    std::atomic<bool> ticked_;

    static constexpr double get_alpha()
    {
        return 1 - exp((TInterval * -1.0l) / (TWindow * 2.0l));
    }

    template<bool TWrite = true>
    TValue tick(const clock_point &at) noexcept;

public:
    ewma(const TClockGet &clock = TClockGet()) noexcept;
    ewma(const ewma &e) noexcept;
    ~ewma() = default;

    template<typename TAmt>
    void mark(TAmt amount) noexcept;

    bool compare_exchange(TValue &expectedrate, TValue rate) noexcept;

    TValue rate() noexcept;

    TValue rate() const noexcept;

    ewma &operator=(const ewma<TClockGet, TWindow, TInterval, TValue> &c) noexcept;

};

template<typename TClockGet, period::value TWindow, period::value TInterval, typename TValue>
long double ewma<TClockGet, TWindow, TInterval, TValue>::alpha_ = ewma<TClockGet, TWindow, TInterval, TValue>::get_alpha();

template<typename TClockGet, period::value TWindow, period::value TInterval, typename TValue>
ewma<TClockGet, TWindow, TInterval, TValue>::ewma(const TClockGet &clock) noexcept :
        clk_(clock),
        rate_(0),
        last_(clk_()),
        pending_(0),
        ticked_(false)
{
}

template<typename TClockGet, period::value TWindow, period::value TInterval, typename TValue>
ewma<TClockGet, TWindow, TInterval, TValue>::ewma(const ewma &c) noexcept :
    clk_(c.clk_),
    rate_(c.rate_.load()),
    last_(c.last_),
    pending_(c.pending_.load()),
    ticked_(c.ticked_.load())
{

}

template<typename TClockGet, period::value TWindow, period::value TInterval, typename TValue>
template<typename TMark>
void ewma<TClockGet, TWindow, TInterval, TValue>::mark(TMark amount) noexcept
{
    auto now = clk_();

    // our clock went backwards
    if (now < last_)
        return;

    tick(now);
    atomic_add(pending_, amount);
}

template<typename TClockGet, period::value TWindow, period::value TInterval, typename TValue>
bool ewma<TClockGet, TWindow, TInterval, TValue>::compare_exchange(TValue &expectedrate, TValue rate) noexcept
{
    return rate_.compare_exchange_weak(expectedrate, rate);
}

template<typename TClockGet, period::value TWindow, period::value TInterval, typename TValue>
TValue ewma<TClockGet, TWindow, TInterval, TValue>::rate() noexcept
{
    auto now = clk_();
    return tick(now);
}

template<typename TClockGet, period::value TWindow, period::value TInterval, typename TValue>
TValue ewma<TClockGet, TWindow, TInterval, TValue>::rate() const noexcept
{
    auto now = clk_();
    // the const_cast is safe with the false template parameter
    return const_cast<ewma*>(this)->tick<false>(now);
}

template<typename TClockGet, period::value TWindow, period::value TInterval, typename TValue>
template<bool TWrite>
TValue ewma<TClockGet, TWindow, TInterval, TValue>::tick(const clock_point &at) noexcept
{
    int missed_intervals;
    clock_point last;

    last = last_;

    auto pending = pending_.load();
    auto nrate = rate_.load();

    if (at < last)
        return nrate;

    if (nrate == TValue{} && !ticked_.load())
    {
        if ((at - last) < period(TInterval))
            return pending;

        bool ticked = false;
        if (ticked_.compare_exchange_weak(ticked, true))
        {
            // use constexpr if with 17
            if (TWrite)
            {
                // one thread sets the last timestamp
                if (!pending_.compare_exchange_weak(pending, 0))
                    return pending; // someone else ticked from under us

                if (rate_.compare_exchange_weak(nrate, pending))
                    last_ = at;
            }

            return pending;
        }
    }

    // apply the pending value to our current rate
    // if someone else already snagged the pending value, start over
    auto rate = nrate + (alpha_ * (pending - nrate));

    // figure out how many intervals we've missed
    missed_intervals = ((at - last) / period(TInterval)) - 1;
    if (missed_intervals > 0)
    {
        // we missed some intervals - we'll average in zeros
        if ((TWindow > TInterval) && (at - last) > period(TWindow))
        {
            constexpr auto intervals_per_window = TWindow / TInterval;
            period::value missed_windows = missed_intervals / intervals_per_window;
            rate = pow(rate, 1/pow(missed_windows, 2));

            // figure out the missed intervals now
            missed_intervals -= (missed_windows * intervals_per_window);
        }

        // this is the fastest way to do this - the alternative is a standard repeated integral
        // but this is faster
        for (int i = 0; i < missed_intervals; i++)
            rate = rate + (alpha_ * -rate);
    }

    if (std::isnan(rate) || std::isinf(rate))
        rate = TValue{};

    // make sure that last_ didn't catch up with us
    if (!TWrite || (at - last) < period(TInterval))
        return rate;

    if (!pending_.compare_exchange_weak(pending, 0))
        return rate; // someone else already either ticked or added a pending value

    rate_.store(rate);
    if (last_ < at)
        last_ = at;

    return rate;
}

template<typename TClockGet, period::value TWindow, period::value TInterval, typename TValue>
ewma<TClockGet, TWindow, TInterval, TValue> &ewma<TClockGet, TWindow, TInterval, TValue>::operator=(const ewma &c) noexcept
{
    alpha_ = c.alpha_;
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
template<period::value TWindow, period::value TInterval = time::seconds(1), typename TValue = double>
class ewma : public metric<ewma<TWindow, TInterval, TValue>>
{
    internal::ewma<steady_clock_point, TWindow, TInterval, TValue> ewma_;
public:
    /**
     * \brief Construct an exponential weighted moving average
     *
     * \param window The window over which the average accounts for
     * \param interval The interval at which the average is calculated
     */
    ewma() noexcept = default;
    ewma(const ewma &ewma) noexcept = default;

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
    typename std::enable_if<std::is_arithmetic<TMark>::value, ewma<TWindow, TInterval, TValue>>::type&
    operator+=(typename std::enable_if<std::is_arithmetic<TMark>::value, TMark>::type value) noexcept
    {
        mark(value);
        return *this;
    }

    /**
     * Get a snapshot of the moving average
     */
    average_value_snapshot snapshot() noexcept
    {
        return average_value_snapshot(ewma_.rate());
    }

    /**
     * Get a snapshot of the moving average
     */
    average_value_snapshot snapshot() const noexcept
    {
        return average_value_snapshot(ewma_.rate());
    }
};

}

#endif //CXXMETRICS_EWMA_HPP
