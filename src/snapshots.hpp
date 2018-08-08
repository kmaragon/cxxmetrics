#ifndef CXXMETRICS_SNAPSHOTS_HPP
#define CXXMETRICS_SNAPSHOTS_HPP

#include <unordered_map>
#include <vector>
#include <algorithm>
#include "metric_value.hpp"

namespace cxxmetrics
{

/**
 * \brief a snapshot type that represents only a single value
 */
class value_snapshot
{
protected:
    metric_value value_;
public:
    /**
     * \brief constructor
     */
    value_snapshot(metric_value&& value) noexcept;

    value_snapshot(value_snapshot&& other) :
            value_(std::move(other.value_))
    { }

    value_snapshot& operator=(value_snapshot&& other)
    {
        value_ = std::move(other.value_);
        return *this;
    }

    /**
     * \brief Get the value in the snapshot
     */
    metric_value value() const;

    /**
     * \brief Comparison operators
     */
    bool operator==(const value_snapshot& other) const noexcept;
    bool operator==(const metric_value& value) const noexcept;
    bool operator!=(const value_snapshot& other) const noexcept;
    bool operator!=(const metric_value& value) const noexcept;
};

inline value_snapshot::value_snapshot(metric_value&& value) noexcept :
        value_(std::move(value))
{ }

inline metric_value value_snapshot::value() const {
    return value_;
}

inline bool value_snapshot::operator==(const value_snapshot &other) const noexcept
{
    return value_ == other.value_;
}

inline bool value_snapshot::operator==(const metric_value &value) const noexcept
{
    return value_ == value;
}

inline bool value_snapshot::operator!=(const value_snapshot &other) const noexcept
{
    return value_ == other.value_;
}

inline bool value_snapshot::operator!=(const metric_value &value) const noexcept
{
    return value_ == value;
}

class cumulative_value_snapshot : public value_snapshot
{
public:
    cumulative_value_snapshot(metric_value&& value) noexcept :
            value_snapshot(std::move(value))
    { }

    void merge(const cumulative_value_snapshot& other) noexcept
    {
        value_ += other.value_;
    }
};

class average_value_snapshot : public value_snapshot
{
    uint64_t samples_;

    average_value_snapshot(metric_value&& value, uint64_t samples) :
            value_snapshot(std::move(value)),
            samples_(samples)
    { }

public:
    average_value_snapshot(metric_value&& value) noexcept :
            average_value_snapshot(std::move(value), 1)
    { }

    average_value_snapshot(average_value_snapshot&& other) noexcept :
            value_snapshot(std::move(other)),
            samples_(other.samples_)
    { }

    average_value_snapshot& operator=(average_value_snapshot&& other)
    {
        value_snapshot::operator=(std::move(other));
        samples_ = other.samples_;
        return *this;
    }

    void merge(const average_value_snapshot& other) noexcept
    {
        auto sb = samples_ * 1.0l;
        auto os = other.samples_ * 1.0l;
        auto s1 = samples_ + os;

        value_ = (metric_value(sb / s1) * value_) + (other.value() * metric_value(os / s1));
        samples_ = s1;
    }
};

class meter_snapshot : public average_value_snapshot
{
    std::unordered_map<std::chrono::steady_clock::duration, metric_value> rates_;
public:
    meter_snapshot(metric_value&& mean, std::unordered_map<std::chrono::steady_clock::duration, metric_value>&& rates) :
            average_value_snapshot(std::move(mean)),
            rates_(std::move(rates))
    { }

    meter_snapshot(meter_snapshot&& other) :
            average_value_snapshot(std::move(other)),
            rates_(std::move(other.rates_))
    { }

    meter_snapshot& operator=(meter_snapshot&& other)
    {
        average_value_snapshot::operator=(std::move(other));
        rates_ = std::move(other.rates_);
        return *this;
    }

    auto begin() const noexcept
    {
        return rates_.begin();
    }

    auto end() const noexcept
    {
        return rates_.end();
    }
};

class quantile
{
    long double value_;
    static constexpr uint64_t max = static_cast<uint64_t>(~(static_cast<uint32_t>(0)));
public:
    using value = uint64_t;

    constexpr quantile(value v) :
            value_((v * 100.0) / max)
    {
    }

    constexpr quantile(long double v) :
            value_(v)
    {
    }

    constexpr operator value()
    {
        return (max / 100.0) * value_;
    }

    constexpr operator long double()
    {
        return value_;
    }

};

namespace literals
{

constexpr quantile operator ""_p(long double value)
{
    return quantile(value);
}

constexpr quantile operator ""_p(long long unsigned int value)
{
    return quantile(value * 1.0l);
}

}

/**
 * A reservoir snapshot from which quantiles, mins, and maxes can be grabbed
 */
class reservoir_snapshot
{
    std::vector<metric_value> values_;
public:
    /**
     * \brief Construct a snapshot using the specified iterators
     *
     * \param begin the beginning of the collection
     * \param end the end of the collection
     * \param size the expected size for the snapshot
     */
    template <class TInputIterator>
    reservoir_snapshot(TInputIterator begin, const TInputIterator &end, std::size_t size) noexcept;

    /**
     * \brief Construct a snapshot with the c style array
     *
     * \param a The array from which to construct the snapshot
     * \param count the number of items in the array
     */
    template<typename TElem>
    reservoir_snapshot(const TElem *a, std::size_t count) noexcept;

    /**
     * \brief Move constructor
     */
    reservoir_snapshot(reservoir_snapshot &&other) noexcept;

    /**
     * \brief Move assignment constructor
     */
    reservoir_snapshot &operator=(reservoir_snapshot &&other) noexcept;

    reservoir_snapshot(const reservoir_snapshot &c) = delete;
    reservoir_snapshot &operator=(const reservoir_snapshot &c) = delete;

    /**
     * \brief Get the value at a specified quantile
     *
     * \tparam TQuantile the quantile for which to get the value. Must be between 0 and 100. For example 99.999_p
     *
     * \return the value at the specified quantile
     */
    template<quantile::value TQuantile>
    metric_value value() const noexcept
    {
        constexpr auto q = ((long double)quantile(TQuantile))/100.0;
        static_assert(q >= 0 && q <= 1, "The provided quantile value is invalid. Must be between 0 and 1");

        if (values_.size() < 1)
            return metric_value(0);

        auto pos = q * (values_.size() + 1);
        auto index = static_cast<int64_t>(pos);

        if (index < 1)
            return min();
        if (static_cast<std::size_t>(index) >= values_.size())
            return max();

        return values_[index - 1] + metric_value((pos - index) * static_cast<long double>(values_[index] - values_[index - 1]));
    }

    /**
     * \brief Get the mean value in the snapshot
     *
     * \return the snapshot mean
     */
    metric_value mean() const noexcept
    {
        metric_value total(0.0l);
        if (values_.empty())
            return total;

        // should we worry about overflow here?
        for (std::size_t i = 0; i < values_.size(); i++)
        {
            auto vs = i + 1.0l;
            long double eratio = i / vs;
            long double nratio = 1.0l / vs;
            total = (total * metric_value(eratio)) + (values_[i] * metric_value(nratio));
        }

        return total;
    }

    /**
     * \brief Get the minimum value in the snapshot
     *
     * \return The minimum value in the snapshot
     */
    metric_value min() const noexcept
    {
        return values_.empty() ? metric_value(std::numeric_limits<int64_t>::min()) : values_[0];
    }

    /**
     * \brief Get the maximum value in the snapshot
     *
     * \return The maximum value in the snapshot
     */
    metric_value max() const noexcept
    {
        return values_.empty() ? metric_value(std::numeric_limits<int64_t>::max()) : values_[values_.size()-1];
    }
};

template<typename TInputIterator>
reservoir_snapshot::reservoir_snapshot(TInputIterator begin, const TInputIterator &end, std::size_t size) noexcept
{
    values_.reserve(size);

    std::size_t at = 0;
    for (; begin != end && at++ < size; ++begin)
        values_.emplace_back(*begin);

    std::sort(values_.begin(), values_.end());
}

template<typename TElem>
reservoir_snapshot::reservoir_snapshot(const TElem *a, std::size_t count) noexcept
{
    values_.reserve(count);

    std::size_t at = 0;
    for (; at < count; ++at)
        values_.emplace_back(a[at]);

    std::sort(values_.begin(), values_.end());
}

inline reservoir_snapshot::reservoir_snapshot(reservoir_snapshot&& other) noexcept :
        values_(std::move(other.values_))
{ }

inline reservoir_snapshot& reservoir_snapshot::operator=(reservoir_snapshot&& other) noexcept
{
    values_ = std::move(other.values_);
    return *this;
}

class histogram_snapshot : public reservoir_snapshot
{
    uint64_t count_;
public:
    histogram_snapshot(reservoir_snapshot&& q, uint64_t count) :
            reservoir_snapshot(std::move(q)),
            count_(std::move(count))
    { }

    histogram_snapshot(histogram_snapshot&& other) :
            reservoir_snapshot(std::move(other)),
            count_(other.count_)
    { }

    histogram_snapshot& operator=(histogram_snapshot&& other)
    {
        reservoir_snapshot::operator=(std::move(other));
        count_ = other.count_;
        return *this;
    }

    uint64_t count() const
    {
        return count_;
    }
};

class timer_snapshot : public histogram_snapshot
{
    meter_snapshot meter_;
public:
    timer_snapshot(histogram_snapshot&& h, meter_snapshot&& m) :
            histogram_snapshot(std::move(h)),
            meter_(std::move(m))
    { }

    timer_snapshot(timer_snapshot&& other) noexcept :
            histogram_snapshot(std::move(other)),
            meter_(std::move(other.meter_))
    { }

    timer_snapshot& operator=(timer_snapshot&& other)
    {
        histogram_snapshot::operator=(std::move(other));
        meter_ = std::move(other.meter_);
        return *this;
    }

    /**
     * \brief Get the rates associated with the timer. The values are the time quantiles and the rates are the rates of items timed
     */
    const meter_snapshot& rate() const
    {
        return meter_;
    }
};

/**
 * \brief A visitor that can react to metric snapshots
 */
class snapshot_visitor
{
public:
    virtual void visit(const value_snapshot& value)
    { }
    virtual void visit(const meter_snapshot& meter)
    { }
    virtual void visit(const histogram_snapshot& hist)
    { }
    virtual void visit(const timer_snapshot& timer)
    {
        visit(static_cast<const histogram_snapshot&>(timer));
        visit(static_cast<const meter_snapshot&>(timer.rate()));
    }
    virtual ~snapshot_visitor() = default;
};

template<typename TVisitor>
class invokable_snapshot_visitor : public snapshot_visitor
{
    TVisitor visitor_;

    template<typename T, typename TSnapshot>
    class has_overload
    {
        template<typename _T>
        static std::true_type check(decltype(std::declval<_T>()(std::declval<TSnapshot>()))*);
        template<typename _T>
        static std::false_type check(...);
    public:
        static constexpr bool value = decltype(check<T>(nullptr))::value;
    };

    class overload_caller
    {
        TVisitor& visitor_;
    public:
        overload_caller(TVisitor& visitor) :
                visitor_(visitor)
        { }

        template<typename TSnapshot>
        typename std::enable_if<has_overload<TVisitor, TSnapshot>::value, void>::type operator()(invokable_snapshot_visitor&, const TSnapshot& ss)
        {
            visitor_(ss);
        }

        template<typename TSnapshot>
        typename std::enable_if<!has_overload<TVisitor, TSnapshot>::value, void>::type operator()(invokable_snapshot_visitor& base, const TSnapshot& ss)
        {
            base.snapshot_visitor::visit(ss);
        }
    };

    template<typename TSnapshot>
    void visit_hnd(const TSnapshot& ss)
    {
        overload_caller caller(visitor_);
        caller(*this, ss);
    }

public:
    invokable_snapshot_visitor(TVisitor&& visitor) :
            visitor_(std::forward<TVisitor>(visitor))
    { }
    void visit(const value_snapshot& value) override { visit_hnd(value); }
    void visit(const meter_snapshot& meter) override { visit_hnd(meter); }
    void visit(const histogram_snapshot& hist) override { visit_hnd(hist); }
    void visit(const timer_snapshot& timer) override { visit_hnd(timer); }
};

}

#endif //CXXMETRICS_SNAPSHOTS_HPP