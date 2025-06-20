#pragma once

#include "counter.hpp"
#include <atomic>

namespace cxxmetrics::state {

/**
 * \brief The counter implementation that is atomic and thread-safe
 */
template <typename T = int64_t>
class atomic_counter : public counter<T> {
  std::atomic<T> value_;

public:
  explicit atomic_counter(T initial_value = 0) noexcept;

  atomic_counter(const atomic_counter &c) noexcept;

  atomic_counter(atomic_counter &&c) noexcept;

  atomic_counter &operator=(const atomic_counter &c) noexcept;
  atomic_counter &operator=(atomic_counter &&c) noexcept;

  T incr(T by) noexcept override;

  T current_value() const noexcept override;

  [[nodiscard]] bool is_atomic() const noexcept override;
};

template <typename T>
atomic_counter<T>::atomic_counter(T initial_value) noexcept
    : value_(initial_value) {}

template <typename T>
atomic_counter<T>::atomic_counter(const atomic_counter &c) noexcept
    : counter<T>(c), value_(c.value_.load(std::memory_order_relaxed)) {}

template <typename T>
atomic_counter<T>::atomic_counter(atomic_counter &&c) noexcept
    : value_source(std::move(c)),
      value_(c.value_.exchange(0, std::memory_order_relaxed)) {}

template <typename T>
atomic_counter<T> &
atomic_counter<T>::operator=(const atomic_counter<T> &c) noexcept {
  value_.store(c.value_.load(std::memory_order_relaxed),
               std::memory_order_relaxed);
  return *this;
}

template <typename T>
atomic_counter<T> &
atomic_counter<T>::operator=(atomic_counter<T> &&c) noexcept {
  value_.store(c.value_.exchange(0, std::memory_order_relaxed),
               std::memory_order_relaxed);
  return *this;
}

template <typename T>
T atomic_counter<T>::incr(T by) noexcept {
  return value_.fetch_add(by, std::memory_order_relaxed) + by;
}

template <typename T>
T atomic_counter<T>::current_value() const noexcept {
  return value_.load(std::memory_order_relaxed);
}

template <typename T>
bool atomic_counter<T>::is_atomic() const noexcept {
  return true;
}

} // namespace cxxmetrics::state
