#pragma once

#include "../state/counter.hpp"
#include "metric_handle.hpp"

namespace cxxmetrics::metrics {

/**
 * \brief The user facing control for a counter
 */
template <typename T>
class counter {
  detail::metric_handle<state::counter<T>> hnd_;

public:
  /**
   * \brief Convenience cast operator
   */
  explicit operator T() const { return hnd_->current_value(); }

  /**
   * \brief increment the counter by the specified value
   *
   * \param by the amount by which to increment the counter
   *
   * \return the value of the counter after the increment
   */
  T incr(T by) noexcept {
    return hnd_->incr(by);
  }

  /**
   * \brief Convenience operator to increment the counter by 1
   *
   * \return a reference to the counter
   */
  counter &operator++() {
    hnd_->incr(1);
    return *this;
  }

  /**
   * \brief Convenience operator to increment the counter by 1 in postfix
   *
   * \return a reference to the counter
   */
  T operator++(int) { return hnd_->incr(1) - 1; }

  /**
   * \brief Convenience operator to increment the counter by a value
   *
   * \param by the amount by which to increment the counter
   *
   * \return a reference to the counter
   */
  counter &operator+=(T by) {
    hnd_->incr(by);
    return *this;
  }

  /**
   * \brief Convenience operator to decrement the counter by 1
   *
   * \return a reference to the counter
   */
  counter &operator--() {
    hnd_->incr(-1);
    return *this;
  }

  /**
   * \brief Convenience operator to decrement the counter by a value
   *
   * \param by the amount by which to decrement the counter
   *
   * \return a reference to the counter
   */
  counter &operator-=(T by) {
    hnd_->incr(-by);
    return *this;
  }
};

} // namespace cxxmetrics::metrics