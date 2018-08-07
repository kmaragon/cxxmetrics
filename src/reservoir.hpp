#ifndef CXXMETRICS_RESERVOIR_HPP
#define CXXMETRICS_RESERVOIR_HPP

#include "metric_value.hpp"
#include <vector>
#include <algorithm>

namespace cxxmetrics
{

class quantile
{
    long double value_;
    static constexpr unsigned long max = ~((unsigned long)0);
public:
    using value = unsigned long;

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
    return quantile((long double) value);
}

}

namespace reservoirs
{

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
    reservoir_snapshot(reservoir_snapshot &&c) noexcept;

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
    for (; begin != end && at < size; ++begin)
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

}

}

#endif //CXXMETRICS_RESERVOIR_HPP
