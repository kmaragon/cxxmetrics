#pragma once

#include "value_gauge.hpp"
#include <atomic>

namespace cxxmetrics::state {

/**
 * \brief The counter implementation that is atomic and thread-safe
 */
template<typename T>
class atomic_value_gauge : public value_gauge<T> {
  std::atomic<T> value_;

public:
  explicit atomic_value_gauge(T initial_value = T{}) noexcept;

  atomic_value_gauge(const atomic_value_gauge &c) noexcept;

  atomic_value_gauge(atomic_value_gauge &&c) noexcept;

  atomic_value_gauge &operator=(const atomic_value_gauge &c) noexcept;
  atomic_value_gauge &operator=(atomic_value_gauge &&c) noexcept;

  void set_value(T new_value) noexcept override;

  T get_value() const noexcept override;

  [[nodiscard]] bool is_atomic() const noexcept override;
};

template<typename T>
atomic_value_gauge<T>::atomic_value_gauge(T initial_value) noexcept
    : value_(initial_value) {}

template<typename T>
atomic_value_gauge<T>::atomic_value_gauge(const atomic_value_gauge &c) noexcept
    : counter<T>(c),
      value_(c.value_.load(std::memory_order_relaxed)) {}

template<typename T>
atomic_value_gauge<T>::atomic_value_gauge(atomic_value_gauge &&c) noexcept
    : value_source(std::move(c)),
      value_(c.value_.exchange(T{}, std::memory_order_relaxed)) {}

template<typename T>
atomic_value_gauge<T> &
atomic_value_gauge<T>::operator=(const atomic_value_gauge<T> &c) noexcept {
  value_.store(
      c.value_.load(std::memory_order_relaxed), std::memory_order_relaxed);
  return *this;
}

template<typename T>
atomic_value_gauge<T> &
atomic_value_gauge<T>::operator=(atomic_value_gauge<T> &&c) noexcept {
  value_.store(
      c.value_.exchange(0, std::memory_order_relaxed),
      std::memory_order_relaxed);
  return *this;
}

template<typename T>
void atomic_value_gauge<T>::set_value(T val) noexcept {
  return value_.store(val, std::memory_order_relaxed);
}

template<typename T>
T atomic_value_gauge<T>::get_value() const noexcept {
  return value_.load(std::memory_order_relaxed);
}

template<typename T>
bool atomic_value_gauge<T>::is_atomic() const noexcept {
  return true;
}

} // namespace cxxmetrics::state
