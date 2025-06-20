#pragma once

#include "source.hpp"

namespace cxxmetrics::state {

/**
 * \brief The base type for a distribution state
 */
template <typename T>
class distribution : public distribution_source {
public:
  /**
   * \brief Update the distribution with a new value
   */
  virtual void update(T value) = 0;
};

} // namespace cxxmetrics::state