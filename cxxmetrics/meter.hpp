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

template<typename TClockGet, period::value TWindow, period::value TInterval>
class rate_counter
{
    ewma<TClockGet, TWindow, TInterval, double> rate_;
public:
    rate_counter(const TClockGet &clock) noexcept :
            rate_(clock)
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

template<typename TClockGet, period::value TInterval, period::value... TWindow>
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
        static constexpr void mark(rates_holder<TClockGet, TInterval, TWindow...> *c, int64_t value)
        {
        }
    };

    template<period::value TPeriod, period::value ...TPeriods>
    struct marker<TPeriod, TPeriods...>
    {
        static constexpr marker<TPeriods...> inner_ = marker<TPeriods...>();
        static constexpr void mark(rates_holder<TClockGet, TInterval, TWindow...> *c, int64_t value)
        {
            c->get_rate<TPeriod>().mark(value);
            inner_.mark(c, value);
        }
    };

    std::tuple<rate_counter<TClockGet, TWindow, TInterval>...> rates_;
public:
    rates_holder(const TClockGet &clock) :
        rates_(rate_counter<TClockGet, TWindow, TInterval>{clock}...)
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

    constexpr void mark(int64_t value)
    {
        marker<TWindow...>::mark(this, value);
    }
};

template<typename TClockGet, period::value TInterval, period::value ... TWindows>
class _meter_impl_base
{
public:
    using clock_point = typename clock_traits<TClockGet>::clock_point;
    using clock_diff = typename clock_traits<TClockGet>::clock_diff;

    rates_holder<TClockGet, TInterval, TWindows...> rates_;

private:
    template<period::value... TPeriods>
    struct each_fn
    {
        template<typename _T>
        constexpr void doeach(_meter_impl_base<TClockGet, TInterval, TWindows...> &on, const _T &fn)
        {
        }

        template<typename _T>
        constexpr void doeach(const _meter_impl_base<TClockGet, TInterval, TWindows...> &on, const _T &fn)
        {
        }
    };

    template<period::value TCur, period::value ...TPeriods>
    struct each_fn<TCur, TPeriods...>
    {
        each_fn<TPeriods...> next;

        template<typename _T>
        constexpr void doeach(_meter_impl_base<TClockGet, TInterval, TWindows...> &on, const _T &fn)
        {
            fn(meter_rate(period(TCur).to_duration(), on.get_rate<TCur>()));
            next.doeach(on, fn);
        }

        template<typename _T>
        constexpr void doeach(const _meter_impl_base<TClockGet, TInterval, TWindows...> &on, const _T &fn)
        {
            fn(meter_rate(period(TCur).to_duration(), on.get_rate<TCur>()));
            next.doeach(on, fn);
        }
    };

    TClockGet clk_;
public:
    explicit _meter_impl_base(const TClockGet &clkget) noexcept :
            rates_(clkget),
            clk_(clkget)
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

    constexpr void mark(int64_t by)
    {
        rates_.mark(by);
    }
protected:
    constexpr clock_diff interval() const noexcept
    {
        return period(TInterval);
    }

    clock_point now() const noexcept
    {
        return clk_();
    }
};

// specialization for track rate mean
template<typename TClockGet, period::value TInterval, period::value ...TWindows>
class _meter_impl : public _meter_impl_base<TClockGet, TInterval, TWindows...>
{
    using clock_point = typename clock_traits<TClockGet>::clock_point;
    clock_point start_;
    std::atomic_int_fast64_t total_;
public:
    explicit _meter_impl(const TClockGet &clkget) noexcept :
            _meter_impl_base<TClockGet, TInterval, TWindows...>(clkget),
            start_{},
            total_(0)
    { }

    _meter_impl(const _meter_impl &c) noexcept :
            _meter_impl_base<TClockGet, TInterval, TWindows...>(c),
            start_(c.start_),
            total_(c.total_.load())
    {

    }

    _meter_impl &operator=(const _meter_impl &c) noexcept
    {
        _meter_impl_base<TClockGet, TInterval, TWindows...>::operator=(c);
        start_ = c.start_;
        total_.store(c.total_.load());

        return *this;
    }

    inline double mean() const noexcept
    {
        auto since = this->now() - start_;
        auto units = (since * 1.0l) / this->interval();
        if (start_ == clock_point{})
            units = 1;
        if (!units)
            return (total_ * 1.0l);
        return (total_ * 1.0l) / units;
    }

    void mark(int64_t by = 1)
    {
        // this will be imperfect but it should be close enough
        if (start_ == clock_point{})
            start_ = this->now();

        _meter_impl_base<TClockGet, TInterval, TWindows...>::mark(by);
        total_ += by;
    }
};

}


namespace meters
{

template<period::value TInterval, period::value ...TWindows>
class meter : public metric<meter<TInterval, TWindows...>>
{
protected:
    internal::_meter_impl<steady_clock_point, TInterval, TWindows...> impl_;
    meter() noexcept;

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

    /**
     * \brief Mark some values in the meter
     *
     * \param by the value to mark the meter by
     */
    inline void mark(int64_t by = 1)
    {
        impl_.mark(by);
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
    constexpr meter_rate rate() noexcept
    {
        return { period(TAt), impl_.template get_rate<TAt>() };
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
    constexpr meter_rate rate() const noexcept
    {
        return { period(TAt), impl_.template get_rate<TAt>() };
    }

    /**
     * \brief Get the mean rate of the meter over the lifespan of tracking
     *
     * \return The meter's lifetime mean
     */
    double mean() const noexcept
    {
        return impl_.mean();
    }

    /**
     * \brief Get a snapshot of the meter values
     *
     * \return a snapshot of the meter values
     */
    meter_snapshot snapshot()
    {
        return meter_snapshot(mean(), this->rates_snapshot());
    }

    /**
     * \brief Get a snapshot of the meter values
     *
     * \return a snapshot of the meter values
     */
    meter_snapshot snapshot() const
    {
        return meter_snapshot(mean(), this->rates_snapshot());
    }
};

template<period::value TInterval, period::value ...TWindows>
meter<TInterval, TWindows...>::meter() noexcept :
    impl_(steady_clock_point())
{ }

template<period::value TInterval, typename TWindows>
class meter_builder;

template<period::value TInterval, period::value ...TWindows>
class meter_builder<TInterval, templates::sortable_template_collection<TWindows...>> : public meter<TInterval, TWindows...>
{
public:
    meter_builder() = default;

    meter_builder(const meter_builder &other) noexcept = default;
    meter_builder &operator=(const meter_builder &other) noexcept = default;
};

}

/**
 * \brief A meter that tracks the lifetime mean along with the rates specified in the template parameters
 *
 * \tparam TWindows The various time windows to track. For example '15_min'
 */
template<period::value TInterval, period::value... TWindows>
class meter : public meters::meter_builder<TInterval, typename templates::sort_unique<TWindows...>::type>
{
    using base = meters::meter_builder<TInterval, typename templates::sort_unique<TWindows...>::type>;
public:
    meter() noexcept = default;
    meter(const meter& m) noexcept  = default;
    meter& operator=(const meter& m) noexcept = default;

};

}

#endif //CXXMETRICS_METER_HPP
