#pragma once
#include "source.hpp"

namespace cxxmetrics::state {

/**
 * A subset of source_type that can apply to a gauge
 */
enum class guage_type {
  mean_value,
  last_value,
  sum_value
};

template<typename T, guage_type Type>
class gauge : public value_source {
public:
  source_type type() const noexcept override {
    if constexpr (Type == guage_type::mean_value) {
      return source_type::mean_value;
    } else if constexpr (Type == guage_type::last_value) {
      return source_type::last_value;
    } else if constexpr (Type == guage_type::sum_value) {
      return source_type::sum_value;
    } else {
      static_assert(false, "Invalid type");
    }
  }

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