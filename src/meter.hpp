#ifndef CXXMETRICS_METER_HPP
#define CXXMETRICS_METER_HPP

#include "ewma.hpp"
#include "time.hpp"

namespace cxxmetrics
{

/**
 * \brief A rate for a particluar window in a meter
 */
struct meter_rate
{
    meter_rate() noexcept = default;
    meter_rate(const std::chrono::steady_clock::duration &d, double r) noexcept :
            period(d), rate(r)
    {

    }

    /**
     * \brief The period over which the meter is tracking
     */
    std::chrono::steady_clock::duration period;

    /**
     * \brief The rate within the period that the meter has measured
     */
    double rate;
};

// Internal stuff for meters
namespace internal
{

template<typename TClockGet, period::value TWindow>
class rate_counter
{
    ewma<TClockGet> rate_;
public:
    rate_counter(const TClockGet &clock, const typename ewma<TClockGet>::clock_diff &interval) noexcept :
            rate_(period(TWindow), interval, clock)
    {
    }

    rate_counter(const rate_counter &c) noexcept = default;
    rate_counter &operator=(const rate_counter &c) noexcept = default;

    void mark(int64_t value) noexcept
    {
        rate_.mark(value);
    }

    constexpr std::chrono::steady_clock::duration window() const noexcept
    {
        return period(TWindow);
    }

    double rate() const noexcept
    {
        return rate_.rate();
    }

    double rate() noexcept
    {
        return rate_.rate();
    }
};

template<typename TClockGet, period::value... TWindow>
class rates_holder
{
    template<period::value TValue, int TStart, period::value ...TPeriods>
    struct find_period;

    template<period::value TValue, int TStart, period::value TCompare, period::value ...TPeriods>
    struct find_period<TValue, TStart, TCompare, TPeriods...>
    {
    public:
        static constexpr int value = (TCompare == TValue) ? TStart : find_period<TValue, TStart + 1, TPeriods...>::value;
    };

    template<period::value TValue, int TStart>
    struct find_period<TValue, TStart>
    {
    public:
        static constexpr int value = -1;
    };

    template<period::value ...TPeriods>
    struct marker
    {
        static constexpr void mark(rates_holder<TClockGet, TWindow...> *c, int64_t value)
        {
        }
    };

    template<period::value TPeriod, period::value ...TPeriods>
    struct marker<TPeriod, TPeriods...>
    {
        static constexpr marker<TPeriods...> inner_ = marker<TPeriods...>();
        static constexpr void mark(rates_holder<TClockGet, TWindow...> *c, int64_t value)
        {
            c->get_rate<TPeriod>().mark(value);
            inner_.mark(c, value);
        }
    };

    std::tuple<rate_counter<TClockGet, TWindow>...> rates_;
public:
    rates_holder(const TClockGet &clock, const typename ewma<TClockGet>::clock_diff &interval) :
        rates_(rate_counter<TClockGet, TWindow>{clock, interval}...)
    {
    }

    rates_holder(const rates_holder &c) noexcept = default;
    rates_holder &operator=(const rates_holder &c) noexcept
    {
        rates_ = c.rates_;
        return *this;
    }

    template<period::value TPeriod>
    constexpr auto &get_rate()
    {
        const int location = find_period<TPeriod, 0, TWindow...>::value;
        static_assert(location >= 0, "The specified time window isn't being tracked in this rate collection");

        return std::get<location>(rates_);
    }

    template<period::value TPeriod>
    constexpr auto &get_rate() const
    {
        const int location = find_period<TPeriod, 0, TWindow...>::value;
        static_assert(location >= 0, "The specified time window isn't being tracked in this rate collection");

        return std::get<location>(rates_);
    }

    void mark(int64_t value)
    {
        marker<TWindow...>::mark(this, value);
    }
};

template<typename TClockGet, period::value ... TWindows>
class _meter_impl_base
{
public:
    using clock_point = typename ewma<TClockGet>::clock_point;
    using clock_diff = typename ewma<TClockGet>::clock_diff;

    rates_holder<TClockGet, TWindows...> rates_;

private:
    template<period::value... TPeriods>
    struct each_fn
    {
        constexpr void doeach(_meter_impl_base<TClockGet, TWindows...> &on, const auto &fn)
        {
        }

        constexpr void doeach(const _meter_impl_base<TClockGet, TWindows...> &on, const auto &fn)
        {
        }
    };

    template<period::value TCur, period::value ...TPeriods>
    struct each_fn<TCur, TPeriods...>
    {
        each_fn<TPeriods...> next;
        constexpr void doeach(_meter_impl_base<TClockGet, TWindows...> &on, const auto &fn)
        {
            fn(meter_rate(period(TCur), on.get_rate<TCur>()));
            next.doeach(on, fn);
        }

        constexpr void doeach(const _meter_impl_base<TClockGet, TWindows...> &on, const auto &fn)
        {
            fn(meter_rate(period(TCur), on.get_rate<TCur>()));
            next.doeach(on, fn);
        }
    };

    TClockGet clk_;
    clock_diff interval_;
public:
    explicit _meter_impl_base(const clock_diff &interval, const TClockGet &clkget) noexcept :
            rates_(clkget, interval),
            clk_(clkget),
            interval_(interval)
    { }

    _meter_impl_base(const _meter_impl_base &b) noexcept = default;
    _meter_impl_base &operator=(const _meter_impl_base &b) noexcept = default;

    template<period::value TPeriod>
    constexpr double get_rate() const
    {
        return rates_.template get_rate<TPeriod>().rate();
    }

    template<period::value TPeriod>
    constexpr double get_rate()
    {
        return rates_.template get_rate<TPeriod>().rate();
    }

    constexpr void each(const auto &fn)
    {
        each_fn<TWindows...> efn;
        efn.doeach(*this, fn);
    }

    constexpr void each(const auto &fn) const
    {
        each_fn<TWindows...> efn;
        efn.doeach(*this, fn);
    }

    virtual void mark(int64_t by)
    {
        rates_.mark(by);
    }
protected:
    constexpr clock_diff interval() const noexcept
    {
        return interval_;
    }

    clock_point now() const noexcept
    {
        return clk_();
    }
};

template<bool TTrackMean, typename TClockGet, period::value ...TWindows>
class _meter_impl;


template<typename TClockGet, period::value ...TWindows>
class _meter_impl<false, TClockGet, TWindows...> : public _meter_impl_base<TClockGet, TWindows...>
{
public:
    explicit _meter_impl(const typename ewma<TClockGet>::clock_diff &interval, const TClockGet &clkget = TClockGet()) noexcept :
            _meter_impl_base<TClockGet, TWindows...>(interval, clkget)
    { }

    _meter_impl(const _meter_impl &c) noexcept = default;
    _meter_impl &operator=(const _meter_impl &c) noexcept = default;
};

// specialization for track rate mean
template<typename TClockGet, period::value ...TWindows>
class _meter_impl<true, TClockGet, TWindows...> : public _meter_impl_base<TClockGet, TWindows...>
{
    typename _meter_impl_base<TClockGet>::clock_point start_;
    std::atomic_int_fast64_t total_;
public:
    explicit _meter_impl(const typename ewma<TClockGet>::clock_diff &interval, const TClockGet &clkget) noexcept :
            _meter_impl_base<TClockGet, TWindows...>(interval, clkget),
            start_(_meter_impl_base<TClockGet, TWindows...>::now()),
            total_(0)
    { }

    _meter_impl(const _meter_impl &c) noexcept :
            _meter_impl_base<TClockGet, TWindows...>(c),
            start_(c.start_),
            total_(c.total_.load())
    {

    }

    _meter_impl &operator=(const _meter_impl &c) noexcept
    {
        _meter_impl_base<TClockGet, TWindows...>::operator=(c);
        start_ = c.start_;
        total_.store(c.total_.load());

        return *this;
    }

    inline double mean() const noexcept
    {
        auto since = _meter_impl_base<TClockGet, TWindows...>::now() - start_;
        auto units = since / _meter_impl_base<TClockGet, TWindows...>::interval();
        if (!units)
            return (total_ * 1.0);
        return (total_ * 1.0) / units;
    }

    void mark(int64_t by) override
    {
        _meter_impl_base<TClockGet, TWindows...>::mark(by);
        total_ += by;
    }
};

}


namespace meters
{

template<bool TWithMean, period::value ...TWindows>
class meter : public metric<meter<TWithMean, TWindows...>>
{
protected:
    internal::_meter_impl<true, steady_clock_point, TWindows...> impl_;
    explicit meter(const std::chrono::steady_clock::duration &interval) noexcept;

    struct map_builder
    {
        mutable std::unordered_map<std::chrono::steady_clock::duration, metric_value> result;

        map_builder() :
                result(sizeof...(TWindows))
        { }

        void operator()(const meter_rate& rate) const {
            result.emplace(rate.period, rate.rate);
        }

        auto rates()
        {
            return std::move(result);
        }

    };

    std::unordered_map<std::chrono::steady_clock::duration, metric_value> rates_snapshot()
    {
        map_builder builder;
        impl_.each(builder);
        return builder.rates();
    }

    std::unordered_map<std::chrono::steady_clock::duration, metric_value> rates_snapshot() const
    {
        map_builder builder;
        impl_.each(builder);
        return builder.rates();
    }
public:
    meter(const meter &m) noexcept = default;
    meter &operator=(const meter &m) noexcept = default;

    inline void mark(int64_t by)
    {
        impl_.mark(by);
    }

    template<period::value TAt>
    constexpr meter_rate rate() noexcept
    {
        return { period(TAt), impl_.template get_rate<TAt>() };
    }

    template<period::value TAt>
    constexpr meter_rate rate() const noexcept
    {
        return { period(TAt), impl_.template get_rate<TAt>() };
    }
};

template<bool TWithMean, period::value ...TWindows>
meter<TWithMean, TWindows...>::meter(const std::chrono::steady_clock::duration &interval) noexcept :
    impl_(interval, steady_clock_point())
{ }

template<bool TWithMean, typename TWindows>
class meter_builder;

template<bool TWithMean, period::value ...TWindows>
class meter_builder<TWithMean, templates::duration_collection<TWindows...>> : public meter<TWithMean, TWindows...>
{
public:
    inline explicit meter_builder(const std::chrono::steady_clock::duration &interval) :
            meter<TWithMean, TWindows...>(interval)
    {
    }

    meter_builder(const meter_builder &other) noexcept = default;
    meter_builder &operator=(const meter_builder &other) noexcept = default;

};

}

/**
 * \brief A meter that tracks the lifetime mean along with the rates specified in the template parameters
 *
 * @tparam TWindows The various time windows to track. For example '15_min'
 */
template<period::value... TWindows>
class meter_with_mean : public meters::meter_builder<true, typename templates::sort_unique<TWindows...>::type>
{
    using base = meters::meter_builder<true, typename templates::sort_unique<TWindows...>::type>;
public:
    /**
     * \brief construct a meter that measures the rates provided using the specified interval
     *
     * \example
     * For example, in order to track the average over a window with a halflife of 15 minutes of transactions per second:
     * \code
     * meter_with_mean<15_min> meter(5_sec);
     * \endcode
     *
     * @param interval the interval that represents the discrete points over which the moving average is applied
     */
    explicit meter_with_mean(const std::chrono::steady_clock::duration &interval = std::chrono::seconds(5)) :
            base(interval)
    {
    }

    meter_with_mean(const meter_with_mean &m) noexcept  = default;
    meter_with_mean &operator=(const meter_with_mean &m) noexcept = default;

    /**
     * \brief Get the mean rate of the meter over the lifespan of tracking
     *
     * @return The meter's lifetime mean
     */
    double mean() const noexcept
    {
        return base::impl_.mean();
    }

    /**
     * \brief Get the rate of a known tracked window
     *
     * If you specify a period that isn't one of the windows provided
     * This will just fail to compile
     *
     * \return The rate measured thus far over the specified window
     */
    template<period::value TAt>
    inline meter_rate rate() const
    {
        return base::template rate<TAt>();
    }

    /**
     * \brief Get the rate of a known tracked window
     *
     * If you specify a period that isn't one of the windows provided
     * This will just fail to compile
     *
     * \return The rate measured thus far over the specified window
     */
    template<period::value TAt>
    inline meter_rate rate()
    {
        return base::template rate<TAt>();
    }

    /**
     * \brief Mark some values in the meter
     *
     * \param by the value to mark the meter by
     */
    inline void mark(int64_t by = 1)
    {
        base::mark(by);
    }

    /**
     * \brief Get a snapshot of the meter values
     *
     * \return a snapshot of the meter values
     */
    meter_with_mean_snapshot snapshot()
    {
        return meter_with_mean_snapshot(mean(), this->rates_snapshot());
    }

    /**
     * \brief Get a snapshot of the meter values
     *
     * \return a snapshot of the meter values
     */
    meter_with_mean_snapshot snapshot() const
    {
        return meter_with_mean_snapshot(mean(), this->rates_snapshot());
    }
};

/**
 * \brief A meter that tracks the lifetime mean along with the rates specified in the template parameters
 *
 * @tparam TWindows The various time windows to track. For example '15_min'
 */
template<period::value... TWindows>
class meter_rates_only : public meters::meter_builder<false, typename templates::sort_unique<TWindows...>::type>
{
    using base = meters::meter_builder<false, typename templates::sort_unique<TWindows...>::type>;
public:
    /**
     * \brief construct a meter that measures the rates provided using the specified interval
     *
     * \example
     * For example, in order to track the average over a window with a halflife of 15 minutes of transactions per second:
     * \code
     * meter_with_mean<15_min> meter(5_sec);
     * \endcode
     *
     * @param interval the interval that represents the discrete points over which the moving average is applied
     */
    explicit meter_rates_only(const std::chrono::steady_clock::duration &interval = std::chrono::seconds(5)) :
            base(interval)
    {
    }

    meter_rates_only(const meter_rates_only &m) noexcept  = default;
    meter_rates_only &operator=(const meter_rates_only &m) noexcept = default;

    /**
     * \brief Get the rate of a known tracked window
     *
     * If you specify a period that isn't one of the windows provided
     * This will just fail to compile
     *
     * \return The rate measured thus far over the specified window
     */
    template<period::value TAt>
    inline meter_rate rate() const
    {
        return base::template rate<TAt>();
    }

    /**
     * \brief Get the rate of a known tracked window
     *
     * If you specify a period that isn't one of the windows provided
     * This will just fail to compile
     *
     * \return The rate measured thus far over the specified window
     */
    template<period::value TAt>
    inline meter_rate rate()
    {
        return base::template rate<TAt>();
    }

    /**
     * \brief Mark some values in the meter
     *
     * \param by the value to mark the meter by
     */
    inline void mark(int64_t by = 1)
    {
        base::mark(by);
    }

    /**
     * \brief Get a snapshot of the meter values
     *
     * \return a snapshot of the meter values
     */
    meter_snapshot snapshot()
    {
        return meter_snapshot(this->rates_snapshot());
    }

    /**
     * \brief Get a snapshot of the meter values
     *
     * \return a snapshot of the meter values
     */
    meter_snapshot snapshot() const
    {
        return meter_snapshot(this->rates_snapshot());
    }
};

}

#endif //CXXMETRICS_METER_HPP
