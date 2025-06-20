#pragma once
#include "source.hpp"

namespace cxxmetrics::state {

template<typename T>
class gauge : public value_source {
protected:
public:

  /**
   * \brief Get the current vlalue
   */
  virtual T get_value() const noexcept = 0;

  /**
   * Get a snapshot of the moving average
   */
  [[nodiscard]] value get() const noexcept override { return get_value(); }
};

} // namespace cxxmetrics::state