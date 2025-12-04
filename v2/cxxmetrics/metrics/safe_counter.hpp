#pragma once

#include "../counter.hpp"
#include <atomic>

namespace cxxmetrics::metrics
{

/**
 * \brief A safe thread-unsafe counter
 *
 * This counter is not thread-safe and should only be used when the counter is incremented from
 * a single thread. And where T is safe to be read from and written to at the same time.
 */
template <typename T>
class safe_counter : public counter<T>
{
    std::atomic<T> value_;

public:
    safe_counter(T initial_value = T{}) noexcept
        : value_(initial_value)
    {
    }

    T incr(T by) noexcept override
    {
        return value_.fetch_add(by, std::memory_order_relaxed) + by;
    }

    T get() const noexcept override
    {
        return value_.load(std::memory_order_relaxed);
    }
};

} // namespace cxxmetrics::metrics
