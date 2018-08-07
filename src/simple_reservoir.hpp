#ifndef CXXMETRICS_SIMPLE_RESERVOIR_HPP
#define CXXMETRICS_XIMPLE_RESERVOIR_HPP

#include "ringbuf.hpp"
#include "snapshots.hpp"

namespace cxxmetrics
{

/**
 * \brief A type of reservoir that simply keep the TSize most recent values
 */
template<typename TElem, size_t TSize>
class simple_reservoir
{
    internal::ringbuf<TElem, TSize> data_;

public:
    simple_reservoir() noexcept = default;

    simple_reservoir(const simple_reservoir &other) noexcept = default;

    ~simple_reservoir() = default;

    /**
     * \brief Assignment operator
     */
    simple_reservoir &operator=(const simple_reservoir &r) noexcept = default;

    /**
     * \brief Update the simple reservoir with a value
     */
    void update(const TElem &v) noexcept;

    /**
     * \brief Get a snapshot of the reservoir
     *
     * \return a reservoir
     */
    reservoir_snapshot snapshot() const noexcept
    {
        return reservoir_snapshot(data_.begin(), data_.end(), TSize);
    }

};

template<typename TElem, size_t TSize>
void simple_reservoir<TElem, TSize>::update(const TElem &v) noexcept
{
    data_.push(v);
}

}

#endif //CXXMETRICS_SIMPLE_RESERVOIR_HPP
