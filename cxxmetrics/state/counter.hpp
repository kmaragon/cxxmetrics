#pragma once

#include "source.hpp"

namespace cxxmetrics::state {

/**
 * \brief The base class for a counter state
 */
template <typename T>
class counter : public value_source {
public:
  /**
   * \brief increment the counter by the specified value
   *
   * \param by the amount by which to increment the counter
   *
   * \return the value of the counter after the increment
   */
  virtual T incr(T by) noexcept = 0;

  /**
   * \brief Get the current value of the counter
   *
   * \return the current value of the counter
   */
  virtual T current_value() const noexcept = 0;

  /**
   * \brief Get a snapshot of the value as it is
   */
  [[nodiscard]] value get() const override { return current_value(); }
};

} // namespace cxxmetrics::state