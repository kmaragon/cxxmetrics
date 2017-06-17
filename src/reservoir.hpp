#ifndef CXXMETRICS_RESERVOIR_HPP
#define CXXMETRICS_RESERVOIR_HPP

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

constexpr quantile operator""_p(long double value)
{
    return quantile(value);
}

constexpr quantile operator""_p(long long unsigned int value)
{
    return quantile((long double)value);
}

namespace reservoirs
{

/**
 * A reservoir snapshot from which quantiles, mins, and maxes can be grabbed
 *
 * \tparam TElem The type of element in the snapshots
 * \tparam TSize The maximum number of elements in the snapshot
 */
template<typename TElem, int TSize>
class snapshot
{
    std::array<TElem, TSize> values_;
    int count_;
public:
    /**
     * \brief Construct a snapshot using the specified iterators
     *
     * \param begin the beginning of the collection
     * \param end the end of the collection
     */
    template <class TInputIterator>
    snapshot(TInputIterator &begin, const TInputIterator &end) noexcept;

    /**
     * \brief Construct a snapshot with the c style array
     *
     * \param a The array from which to construct the snapshot
     * \param count the number of items in the array
     */
    snapshot(const TElem *a, int count) noexcept;

    /**
     * \brief Move constructor
     */
    snapshot(snapshot &&c) noexcept = default;

    /**
     * \brief Move assignment constructor
     */
    snapshot &operator=(snapshot &&other) noexcept = default;

    snapshot(const snapshot &c) = delete;
    snapshot &operator=(const snapshot &c) = delete;

    /**
     * \brief Get the value at a specified quantile
     *
     * \tparam TQuantile the quantile for which to get the value. Must be between 0 and 100. For example 99.999_p
     *
     * \return the value at the specified quantile
     */
    template<quantile::value TQuantile>
    auto value() const noexcept
    {
        constexpr auto q = ((long double)quantile(TQuantile))/100.0;
        static_assert(q >= 0 && q <= 1, "The provided quantile value is invalid. Must be between 0 and 1");

        if (count_ < 1)
            return (long double)0;

        auto pos = q * (count_ + 1);
        int index = (int)pos;

        if (index < 1)
            return (long double)min();
        if (index >= count_)
            return (long double)max();

        return values_[index - 1] + (pos - index) * (values_[index] - values_[index - 1]);
    }

    /**
     * \brief Get the mean value in the snapshot
     *
     * \return the snapshot mean
     */
    auto mean() const noexcept
    {
        if (count_ < 1)
            return (long double)0;

        TElem total = 0;
        for (int i = 0; i < count_; i++)
            total += values_[i];

        return total / (count_ * 1.0l);
    }

    /**
     * \brief Get the minimum value in the snapshot
     *
     * \return The minimum value in the snapshot
     */
    inline TElem min() const noexcept
    {
        return count_ < 1 ? 0 : values_[0];
    }

    /**
     * \brief Get the maximum value in the snapshot
     *
     * \return The maximum value in the snapshot
     */
    inline TElem max() const noexcept
    {
        return count_ < 1 ? 0 : values_[count_-1];
    }
};

template<typename TElem, int TSize>
template<typename TInputIterator>
snapshot<TElem, TSize>::snapshot(TInputIterator &begin, const TInputIterator &end) noexcept
{
    int at = 0;
    for (; begin != end && at < TSize; ++begin)
        values_[at++] = *begin;

    count_ = at;
    std::sort(values_.begin(), values_.begin()+count_);
}

template<typename TElem, int TSize>
snapshot<TElem, TSize>::snapshot(const TElem *a, int count) noexcept
{
    int at = 0;
    for (; at < TSize && at < count; ++at)
        values_[at] = a[at];

    count_ = at;
    std::sort(values_.begin(), values_.begin()+count_);
}

}

}

#endif //CXXMETRICS_RESERVOIR_HPP
