#ifndef CXXMETRICS_TIMER_HPP
#define CXXMETRICS_TIMER_HPP

#include "histogram.hpp"
#include "meter.hpp"
#include "uniform_reservoir.hpp"

namespace cxxmetrics
{

/**
 * \brief A timer that tracks lengths of times the things take
 *
 * \tparam TClock The type of clock to use for tracking wall-clock time for operations. This should be a std::chrono clock type
 * \tparam TReservoir The type of reservoir to use for the underlying histogram of timings
 * \tparam TWindows The meter periods to track rates of, for example: 10_sec, 1_min, 1_hour
 */
template<typename TClock = std::chrono::system_clock, typename TReservoir = uniform_reservoir<typename TClock::duration, 1024>, bool TWithMean = true, period::value... TWindows>
class timer : public metric<timer<TClock, TReservoir, TWithMean, TWindows...>>
{
    histogram<typename TClock::duration, TReservoir> histogram_;
    typename std::conditional<TWithMean, meter_with_mean<TWindows...>, meter_rates_only<TWindows...>>::type meter_;
    TClock clock_;

public:
    using duration = typename TClock::duration;
    using time_point = typename TClock::time_point;
    using clock = TClock;

    timer() = default;

    /**
     * \brief Construct a timer with a reservoir and a clock instance
     *
     * This will usually not get used. It is provided to provide special clocks with std::chrono semantics but rely on internal state
     *
     * \param reservoir The reservoir to use for the timer
     * \param clock the clock to use for tracking wall-clock time
     */
    timer(TReservoir &&reservoir, TClock &&clock = TClock()) :
            histogram_(std::forward<TReservoir>(reservoir)),
            clock_(std::forward<TReservoir>(clock)) {}

    /**
     * \brief Construct a timer with a reservoir instance
     *
     * \param reservoir the reservoir backing the timer
     */
    timer(TReservoir &&reservoir) :
            histogram_(std::forward<TReservoir>(reservoir)) {}

    /**
     * \brief Get the mean throughput of timer updates
     *
     * \return the mean throughput of timer updates
     */
    template<typename = typename std::enable_if<TWithMean>::type>
    double mean() const noexcept
    {
        return meter_.mean();
    }

    /**
     * \brief Get all of the rates defined in the TWindows template parameter
     *
     * \return a collection of rates and the duration to which they correspond
     */
    auto rates_collection() const noexcept
    {
        return meter_.rates_collection();
    }

    /**
     * \brief Get the rate at in a particular window
     *
     * TAt needs to be one of the values provided in TWindow otherwise compilation will fail. For example:
     * \code
     * t.rate<5_sec>();
     * \endcode
     *
     * \tparam TAt The window for which to get the rate
     *
     * \return the rate at the specified window.
     */
    template<period::value TAt>
    auto rate() const
    {
        return meter_.template rate<TAt>();
    }

    /**
     * \brief get the total number of items that have been logged in the timer
     *
     * \return the total count of items in the counter
     */
    uint64_t count() const noexcept
    {
        return histogram_.count();
    }

    /**
     * \brief Log a time in the timer
     *
     * \param duration the duration to log
     */
    void update(const typename TClock::duration &duration) noexcept
    {
        if (duration.count()) {
            histogram_.update(duration);
            meter_.mark();
        }
    }

    /**
     * \brief Executable an invokable and time it.
     *
     * \tparam TRunnable The auto-deduced type of invokable
     * \tparam TIncludeExceptions Whether or not to still log the timer if an exception is thrown. The default is not to
     *
     * \param runnable the invokable object
     *
     * \return whatever the provided invokable returns
     */
    template<typename TRunnable, bool TIncludeExceptions = false>
    typename std::invoke_result<TRunnable>::type time(const TRunnable &runnable);

    /**
     * \brief Get the underlying clock instance
     *
     * \return the underlying clock instance
     */
    const TClock& clock() const noexcept
    {
        return clock_;
    }

    /**
     * \brief Get a snapshot of all of the actual time metrics.
     *
     * From here you can get timing percentiles and mins, maxes, and means. They will all be of the duration type.
     *
     * For example:
     * \code
     * auto ss = mytimer.snapshot();
     * std::cout << "P99: " << format_duration(ss.value<99_p>()) << std::endl;
     * \endcode
     *
     * \return a snapshot of the time metrics
     */
    auto snapshot() const
    {
        return histogram_.snapshot();
    }
};

/**
 * \brief A timer that logs the time since it was constructed upon exiting scope
 *
 * \tparam TTimer the type of timer that will be logged to
 */
template<typename TTimer>
class scoped_timer
{
    TTimer& timer_;
    std::optional<typename TTimer::time_point> start_;

public:
    /**
     * \brief Construct a scoped_timer that will log into a provided Timer instance
     *
     * \param timer the timer to log to when going out of scope
     */
    scoped_timer(Timer& timer) :
            timer_(timer),
            start_(timer.clock().now())
    { }

    scoped_timer(const scoped_timer&) = delete;
    scoped_timer(scoped_timer&& other) noexcept :
            timer_(other.timer_),
            start_(other.start_)
    {
        other.clear();
    }

    ~scoped_timer()
    {
        if (start_)
            timer_.update(timer_.clock().now() - *start_);
    }

    /**
     * \brief Clear the state of the timer so it won't log anything
     */
    void clear() noexcept
    {
        start_.reset();
    }

    /**
     * \brief Reset the timer so that it considers it's start right at the time of the function being called
     */
    void reset()
    {
        start_ = timer.clock().now();
    }
};

template<typename TClock, typename TReservoir, bool TWithMean, period::value... TWindows>
template<typename TRunnable, bool TIncludeExceptions = false>
typename std::invoke_result<TRunnable>::type timer<TClock, TReservoir, TWithMean, TWindows...>::time(const TRunnable &runnable)
{
    scoped_timer<timer<TClock, TReservoir, TWithMean, TWindows...>> tm(*this);
    if (!TIncludeExceptions)
    {
        try
        {
            return runnable();
        }
        catch (...)
        {
            tm.clear();
            throw;
        }
    }

    return runnable();
}

};
#endif //CXXMETRICS_TIMER_HPP
