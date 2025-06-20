#pragma once

#include "value.hpp"
#include <vector>

namespace cxxmetrics {

class distribution;

/**
 * \brief A literal histogram
 */
class histogram {
public:
  /**
   * \brief Make sure the distribution is right sized to support a number of
   * values
   */
  void reserve(std::size_t count);

  /**
   * \brief Clear the histogram
   */
  void clear();

  /**
   * \brief Get the number of buckets in the histogram
   */
  std::size_t size() const;

  auto begin() const { return values_.begin(); }
  auto end() const { return values_.end(); }

private:
  std::vector<std::pair<value, std::size_t>> values_;

  friend class distribution;
};

} // namespace cxxmetrics