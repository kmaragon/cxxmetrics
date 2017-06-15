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

template<typename TClockGet>
class ewma
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
    using clock_point = typename std::decay<decltype(ewma<TClockGet>::clk_point_())>::type;
    using clock_diff = typename std::decay<decltype(ewma<TClockGet>::clk_diff_())>::type;
private:
    TClockGet clk_;
    double alpha_;
    clock_diff interval_;
    clock_diff window_;
    std::atomic<double> rate_;
    clock_point last_;
    std::atomic_int_fast64_t pending_;

    static double get_alpha(const clock_diff &interval, const clock_diff &window)
    {
        return exp((interval * -1.0) / window);
    }

    void tick(const clock_point &at) noexcept;

public:
    ewma(const clock_diff &window, const clock_diff &interval, const TClockGet &clock = TClockGet()) noexcept;
    ewma(const ewma &e) noexcept;
    ~ewma() = default;

    void mark(int64_t amount) noexcept;

    bool compare_exchange(double expectedrate, double rate) noexcept;

    double rate() noexcept;

    double rate() const noexcept;

    ewma &operator=(const ewma<TClockGet> &c) noexcept;

};

template<typename TClockGet>
ewma<TClockGet>::ewma(const clock_diff &window, const clock_diff &interval, const TClockGet &clock) noexcept :
        clk_(clock),
        alpha_(get_alpha(interval, window)),
        interval_(interval),
        window_(window),
        rate_(-1.0),
        pending_(0)
{
    last_ = clk_();
}

template<typename TClockGet>
ewma<TClockGet>::ewma(const ewma<TClockGet> &c) noexcept :
    clk_(c.clk_),
    alpha_(c.alpha_),
    interval_(c.interval_),
    window_(c.window_),
    rate_(c.rate_),
    last_(c.last_),
    pending_(c.pending_)
{

}

template<typename TClockGet>
void ewma<TClockGet>::mark(int64_t amount) noexcept
{
    auto now = clk_();

    // our clock went backwards
    if (now < last_)
        return;

    // See if we crossed the interval threshold. If so we need to tick
    if ((now - last_) >= interval_)
        tick(now);

    pending_ += amount;
}

template<typename TClockGet>
bool ewma<TClockGet>::compare_exchange(double expectedrate, double rate) noexcept
{
    return rate_.compare_exchange_strong(expectedrate, rate, std::memory_order_acq_rel);
}

template<typename TClockGet>
double ewma<TClockGet>::rate() noexcept
{
    double rate = rate_.load(std::memory_order_acq_rel);
    if (rate < 0)
        return 0;

    auto now = clk_();
    if ((now - last_) >= interval_)
        tick(now);

    return rate;
}

template<typename TClockGet>
double ewma<TClockGet>::rate() const noexcept
{
    double rate = rate_.load(std::memory_order_relaxed);
    if (rate < 0)
        return 0;

    return rate;
}

template<typename TClockGet>
void ewma<TClockGet>::tick(const clock_point &at) noexcept
{
    while (true)
    {
        int64_t pending;
        double rate;
        int missed_intervals;
        clock_point last;

        cxxmetrics_ewma_startover:
        pending = pending_.load(std::memory_order_acquire);
        rate = rate_.load(std::memory_order_acquire);
        last = last_;
        if (pending == 0 && rate < 0)
        {
            // one thread sets the last timestamp
            if (rate_.compare_exchange_strong(rate, 0, std::memory_order_release))
                last_ = at;
            return;
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
                rate = 0;
            else
            {
                for (int i = 0; i < missed_intervals; i++)
                    rate = rate + (alpha_ * -rate);
            }
        }

        if (!pending_.compare_exchange_strong(pending, 0, std::memory_order_release))
            goto cxxmetrics_ewma_startover;

        rate_.store(rate, std::memory_order_release);
        last_ = at;
        return;
    }
}

template<typename TClockGet>
ewma<TClockGet> &ewma<TClockGet>::operator=(const ewma<TClockGet> &c) noexcept
{
    alpha_ = c.alpha_;
    interval_ = c.interval_;
    window_ = c.window_;
    rate_.store(c.rate_.load(std::memory_order_acq_rel), std::memory_order_relaxed);
    pending_.store(c.pending_.load(std::memory_order_acq_rel), std::memory_order_relaxed);
    last_ = c.last_;
    return *this;
}
}

/**
 * \brief A default TClockGet implementation using the system steady_clock
 */
struct steady_clock_point
{
    std::chrono::steady_clock::time_point operator()() noexcept
    {
        return std::chrono::steady_clock::now();
    }
};

/**
 * \brief An exponential weighted moving average metric
 */
class ewma : public metric<ewma>
{
    internal::ewma<steady_clock_point> ewma_;
public:
    /**
     * \brief Construct an exponential weighted moving average
     *
     * \param window The window over which the average accounts for
     * \param interval The interval at which the average is calculated
     */
    explicit ewma(std::chrono::steady_clock::duration window,
         std::chrono::steady_clock::duration interval = std::chrono::seconds(5)) noexcept;

    ewma(const ewma &ewma) noexcept = default;
    virtual ~ewma() = default;

    ewma &operator=(const ewma &e) noexcept = default;

    /**
     * \brief Mark the value in the ewma
     *
     * \param value the value to mark in the ewma
     */
    virtual void mark(int64_t value) noexcept;

    /**
     * \brief Get the current rate in the ewma
     *
     * \return the rate of the ewma
     */
    double rate() const noexcept;

    /**
     * \brief Get the current rate in the ewma
     *
     * \return The rate of the ewma
     */
    double rate() noexcept;

    /**
     * \brief Convenience operator to mark a value in the moving average
     *
     * \param value the amount to mark
     * \return a reference to the ewma
     */
    inline ewma &operator+=(int64_t value)
    {
        mark(value);
        return *this;
    }

protected:
    bool compare_exchange(double expectedrate, double rate);
};

}

#endif //CXXMETRICS_EWMA_HPP
