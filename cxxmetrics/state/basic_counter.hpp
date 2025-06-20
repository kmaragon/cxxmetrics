#pragma once

#include "counter.hpp"
#include <atomic>

namespace cxxmetrics::state {

/**
 * \brief The counter implementation that is not thread safe and not atomic
 */
template <typename T = int64_t>
class basic_counter : public counter<T> {
  T value_;

public:
  explicit basic_counter(T initial_value = 0) noexcept;

  basic_counter(const basic_counter &c) noexcept;

  basic_counter(basic_counter &&c) noexcept;

  basic_counter &operator=(const basic_counter &c) noexcept;
  basic_counter &operator=(basic_counter &&c) noexcept;

  T incr(T by) noexcept override;

  T current_value() const noexcept override;

  [[nodiscard]] bool is_atomic() const noexcept override;
};

template <typename T>
basic_counter<T>::basic_counter(T initial_value) noexcept
    : value_(initial_value) {}

template <typename T>
basic_counter<T>::basic_counter(const basic_counter &c) noexcept
    : counter<T>(c), value_(c.value_) {}

template <typename T>
basic_counter<T>::basic_counter(basic_counter &&c) noexcept
    : value_source(std::move(c)), value_(std::move(c.value_)) {
  c.value_ = 0;
}

template <typename T>
basic_counter<T> &
basic_counter<T>::operator=(const basic_counter<T> &c) noexcept {
  value_ = c.value_;
  return *this;
}

template <typename T>
basic_counter<T> &basic_counter<T>::operator=(basic_counter<T> &&c) noexcept {
  value_ = std::move(c.value_);
  c.value_ = 0;
  return *this;
}

template <typename T>
T basic_counter<T>::incr(T by) noexcept {
  return value_ += by;
}

template <typename T>
T basic_counter<T>::current_value() const noexcept {
  return value_;
}

template <typename T>
bool basic_counter<T>::is_atomic() const noexcept {
  return false;
}

} // namespace cxxmetrics::state
