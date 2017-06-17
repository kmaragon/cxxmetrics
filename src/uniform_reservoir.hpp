#ifndef CXXMETRICS_UNIFORM_RESERVOIR_HPP
#define CXXMETRICS_UNIFORM_RESERVOIR_HPP

#include "reservoir.hpp"
#include <atomic>
#include <chrono>
#include <random>

namespace cxxmetrics
{

/**
 * \brief a Uniform Reservoir for getting percentile samples
 *
 * \tparam TElem the type of elements in the reservoir
 * \tparam TSize the size of the reservoir
 */
template<typename TElem, int64_t TSize>
class uniform_reservoir
{
    std::default_random_engine gen_;
    std::array<TElem, TSize> elems_;
    std::atomic_int_fast64_t count_;

    static unsigned generate_seed() noexcept
    {
        auto full = std::chrono::system_clock::now().time_since_epoch();
        auto nano = std::chrono::duration_cast<std::chrono::nanoseconds>(full);

        auto seed = nano.count();
        int size = sizeof(seed);
        while (size > sizeof(unsigned))
        {
            seed = (seed & 0xffffffff) ^ (seed >> 32);
            size -= 4;
        }

        return seed;
    }
public:
    /**
     * \brief Construct a uniform reservoir
     */
    uniform_reservoir() noexcept;

    /**
     * \brief Copy constructor
     */
    uniform_reservoir(const uniform_reservoir &r) noexcept;
    ~uniform_reservoir() = default;

    /**
     * \brief Assignment operator
     */
    uniform_reservoir &operator=(const uniform_reservoir &other) noexcept;

    /**
     * \brief Update the unform reservoir with a value
     */
    void update(TElem) noexcept;

    /**
     * \brief Get a snapshot of the reservoir
     *
     * \return a reservoir
     */
    inline auto snapshot() const noexcept
    {
        return reservoirs::snapshot<TElem, TSize>(elems_.begin(), std::min(count_.load(), TSize));
    }
};

template<typename TElem, int64_t TSize>
uniform_reservoir<TElem, TSize>::uniform_reservoir() noexcept :
        gen_(generate_seed()),
        count_(0)
{
}

template<typename TElem, int64_t TSize>
uniform_reservoir<TElem, TSize>::uniform_reservoir(const uniform_reservoir &other) noexcept :
        gen_(generate_seed()),
        elems_(other.elems_),
        count_(other.count_.load())
{
}

template<typename TElem, int64_t TSize>
uniform_reservoir<TElem, TSize> &uniform_reservoir<TElem, TSize>::operator=(const uniform_reservoir &other) noexcept
{
    elems_ = other.elems_;
    count_ = other.count_;
    return *this;
}

template<typename TElem, int64_t TSize>
void uniform_reservoir<TElem, TSize>::update(TElem value) noexcept
{
    auto c = count_.fetch_add(1);

    if (c < elems_.size())
    {
        elems_[c] = value;
        return;
    }

    // so we don't run out of count
    count_.store(TSize);

    std::uniform_int_distribution<> d(0, TSize);
    elems_[d(gen_)] = value;
}

}

#endif //CXXMETRICS_UNIFORM_RESERVOIR_HPP
