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
    metric_value value_;
public:
    /**
     * \brief constructor
     */
    value_snapshot(const metric_value& value) noexcept;

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

inline value_snapshot::value_snapshot(const metric_value &value) noexcept :
        value_(value)
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
    cumulative_value_snapshot(const metric_value& value) noexcept :
            value_snapshot(value)
    { }
};

class average_value_snapshot : public value_snapshot
{
    uint64_t samples_;

public:
    average_value_snapshot(const metric_value& value) noexcept :
            value_snapshot(value),
            samples_(1)
    { }
};

class meter_snapshot
{
    std::unordered_map<std::chrono::steady_clock::duration, metric_value> rates_;
public:
    meter_snapshot(std::unordered_map<std::chrono::steady_clock::duration, metric_value>&& rates) :
            rates_(std::move(rates))
    { }

    auto begin() const noexcept
    {
        return rates_.begin();
    }

    auto end() const noexcept
    {
        return rates_.end();
    }
};

class meter_with_mean_snapshot : public meter_snapshot, public value_snapshot
{
public:
    meter_with_mean_snapshot(const metric_value& mean, std::unordered_map<std::chrono::steady_clock::duration, metric_value>&& rates) :
            meter_snapshot(std::move(rates)),
            value_snapshot(mean)
    { }
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

    uint64_t count() const
    {
        return count_;
    }
};

template<bool TWithMean>
class timer_snapshot : public histogram_snapshot
{
public:
    using meter_type = typename std::conditional<TWithMean, meter_with_mean_snapshot, meter_snapshot>::type;

private:
    meter_type meter_;
public:
    timer_snapshot(histogram_snapshot&& h, meter_type&& m) :
            histogram_snapshot(std::move(h)),
            meter_(std::move(m))
    { }

    /**
     * \brief Get the rates associated with the timer. The values are the time quantiles and the rates are the rates of items timed
     */
    const meter_type& rate() const
    {
        return meter_;
    }
};

}

#endif //CXXMETRICS_SNAPSHOTS_HPP
