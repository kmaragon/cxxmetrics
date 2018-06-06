#ifndef CXXMETRICS_TIMER_HPP
#define CXXMETRICS_TIMER_HPP

#include "counter.hpp"
#include "uniform_reservoir.hpp"

namespace cxxmetrics
{

template<typename TElem, typename TReservoir = uniform_reservoir<TElem, 1024>>
class histogram
{
    TReservoir reservoir_;
    counter<uint64_t> count_;

public:
    histogram() = default;
    histogram(TReservoir&& reservoir) :
            reservoir_(std::forward<TReservoir>(reservoir))
    { }

    void update(const TElem& value) noexcept
    {
        ++count_;
        reservoir_.update(value);
    }

    uint64_t count() const noexcept
    {
        return static_cast<uint64_t>(count_);
    }

    auto snapshot() const noexcept
    {
        return reservoir_.snapshot();
    }
};

}
#endif //CXXMETRICS_TIMER_HPP
