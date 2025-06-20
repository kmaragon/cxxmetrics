#pragma once
#include "source.hpp"

namespace cxxmetrics::state {

template<typename T>
class ewma : public value_source {
protected:
public:
  /**
   * \brief Mark the value in the ewma
   *
   * \param value the value to mark in the ewma
   */
  virtual void mark(T value) noexcept = 0;

  /**
   * \brief Get the current rate in the ewma
   *
   * \return the rate of the ewma
   */
  virtual T rate() const noexcept = 0;

  /**
   * \brief Get the current rate in the ewma
   *
   * \return The rate of the ewma
   */
  virtual T rate() noexcept = 0;

  /**
   * \brief Get the interval of the ewma
   *
   * This is important because two ewma values with different intervals are
   * not comparable or combinable. But over different windows, that's just two
   * measurements of the same thing.
   */
  [[nodiscard]] virtual std::chrono::nanoseconds interval() const noexcept = 0;

  /**
   * Get a snapshot of the moving average
   */
  [[nodiscard]] value get() const noexcept override { return rate(); }
};

} // namespace cxxmetrics::state