#pragma once

#include "../state/ewma.hpp"
#include "metric_handle.hpp"

namespace cxxmetrics::metrics {

/**
 * \brief The user facing control for a ewma
 */
template<typename T>
class ewma {
  detail::metric_handle<state::ewma<T>> hnd_;

public:
  /**
   * \brief Convenience cast operator
   */
  explicit operator T() const { return hnd_->rate(); }

  /**
   * \brief Mark the value in the ewma
   *
   * \param value the value to mark in the ewma
   */
  void mark(T by) noexcept { return hnd_->mark(by); }

  /**
   * \brief Get the current rate in the ewma
   *
   * \return the rate of the ewma
   */
  virtual T rate() const noexcept { return hnd_->rate(); }

  /**
   * \brief Convenience operator to mark 1 in the moving average
   *
   * \return the rate of the ewma before the mark
   */
  ewma &operator++() {
    hnd_->mark(static_cast<T>(1));
    return *this;
  }

  /**
   * \brief Convenience operator to mark 1 in the moving average
   *
   * \return the rate of the ewma before the mark
   */
  T operator++(int) {
    auto r = hnd_->rate();
    hnd_->mark(static_cast<T>(1));
    return r;
  }

  /**
   * \brief Convenience operator to mark a value in the moving average
   *
   * \param value the amount to mark
   * \return a reference to the ewma
   */
  ewma &operator+=(T by) {
    hnd_->mark(by);
    return *this;
  }
};

} // namespace cxxmetrics::metrics