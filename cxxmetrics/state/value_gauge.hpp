#pragma once
#include "gauge.hpp"

namespace cxxmetrics::state {

template<typename T>
class value_gauge : public gauge<T> {
protected:
public:
  /**
   * \brief Set the value gauge value
   *
   * \param value the value to set
   */
  virtual void set_value(T value) noexcept = 0;
};

} // namespace cxxmetrics::state