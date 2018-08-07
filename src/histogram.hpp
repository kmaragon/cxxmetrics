#ifndef CXXMETRICS_HISTOGRAM_HPP
#define CXXMETRICS_HISTOGRAM_HPP

#include "counter.hpp"
#include "uniform_reservoir.hpp"

namespace cxxmetrics
{

/**
 * \brief A Histogram wrapped over a reservoir
 *
 * The extra feature of the histogram is that it keeps a permanent count
 *
 * \tparam TElem the type of element in the histogram
 * \tparam TReservoir the type of reservoir backing the histogram
 */
template<typename TElem, typename TReservoir = uniform_reservoir<TElem, 1024>>
class histogram
{
    TReservoir reservoir_;
    counter<uint64_t> count_;

public:
    histogram() = default;

    /**
     * \brief Construct a histogram from an existing reservoir
     *
     * \param reservoir the reservoir that willl be backing the histogram
     */
    histogram(TReservoir&& reservoir) :
            reservoir_(std::forward<TReservoir>(reservoir))
    { }

    /**
     * \brief Add a value to the reservoir
     *
     * \param value the value to add
     */
    void update(const TElem& value) noexcept
    {
        ++count_;
        reservoir_.update(value);
    }

    /**
     * \brief Get the total count of items inserted into the reservoir
     *
     * \return the total count of the items inserted into the reservoir
     */
    uint64_t count() const noexcept
    {
        return static_cast<uint64_t>(count_);
    }

    /**
     * \brief Get a snapshot of the histogram
     *
     * \return a snapshot of the histogram
     */
    histogram_snapshot snapshot() const noexcept
    {
        auto c = count_.value();
        return histogram_snapshot(reservoir_.snapshot(), c);
    }
};

}
#endif //CXXMETRICS_HISTOGRAM_HPP
