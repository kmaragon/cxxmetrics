#pragma once

#include "../counter.hpp"

namespace cxxmetrics::metrics
{

/**
 * \brief A simple thread-unsafe counter
 *
 * This counter is not thread-safe and should only be used when the counter is incremented from
 * a single thread. And where T is safe to be read from and written to at the same time.
 */
template <typename T>
class simple_counter : public counter<T>
{
    T value_;

public:
    simple_counter(T initial_value = T{}) noexcept
        : value_(initial_value)
    {
    }

    T incr(T by) noexcept override
    {
        return value_ += by;
    }

    T get() const noexcept override
    {
        return value_;
    }
};

} // namespace cxxmetrics::metrics
