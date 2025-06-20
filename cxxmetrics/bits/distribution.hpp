#pragma once

#include "histogram.hpp"
#include "value.hpp"
#include <vector>

namespace cxxmetrics {

/**
 * \brief A distribution container that sources populate
 */
class distribution {
  void append(value &&v) {
    values_.emplace_back(std::move(v));
    mod_ = true;
  }

public:
  /**
   * \brief Default constructor
   */
  distribution();

  /**
   * \brief Make sure the distribution is right sized to support a number of
   * values
   */
  void reserve(std::size_t count);

  /**
   * \brief Clear the distribution
   */
  void clear();

  /**
   * \brief Get the size of the samples in the distribution
   */
  std::size_t size() const;

  /**
   * \brief Append a value to the distribution
   */
  template <typename T>
  void emplace_back(
      std::enable_if_t<std::is_constructible<value, T &&>::value, T &&> val) {
    append(value{val});
  }

  /**
   * \brief Get the provided quantile from the distribution
   */
  value quantile(float quantile) const;

  /**
   * \brief Get a histogram from the values
   */
  void to_histogram(histogram &h, std::size_t bucket_count) const;

  /**
   * \brief Get the apdex value for the given value
   */
  float apdex(const value &v) const;

private:
  std::vector<value> values_;
  bool mod_;
};

} // namespace cxxmetrics