#pragma once

#include "../bits/distribution.hpp"
#include "../bits/value.hpp"

namespace cxxmetrics::state {

/**
 * \brief The source type
 */
enum class source_type {
  /// \brief A value that aggregates by just taking the "latest" value as best
  /// as can be determined
  snapshot,
  /// \brief A value that aggregates by taking a mean of all the matching tags
  mean,
  /// \brief A value that is monotonically increasing
  monotonic,
  /// \brief A cumulative value that may increase or decrease
  cumulative,
  /// \brief A distribution of values from which quantiles and true histograms
  /// can be derived
  distribution_value
};

/**
 * \brief A Source for a metric value as a component of a broader metric
 */
class source {
public:
  virtual ~source() = default;

  /**
   * \brief Get the source type
   *
   * A distribution type will be a distribution_source. Anything else will be a
   * value_source
   */
  [[nodiscard]] virtual source_type type() const noexcept = 0;

  /**
   * \brief Get whether the source is atomic and thread safe
   *
   * Sources that are not thread safe are not allowed to have multiple handles
   * to them. They are designed to not be shared. They will have less of a
   * performance impact but will not play well in multi-threaded environments.
   */
  [[nodiscard]] virtual bool is_atomic() const = 0;
};

/**
 * \brief A source that only has a single value
 */
class value_source : public source {
public:
  /**
   * \brief Get the single value for the source
   */
  virtual value get() const = 0;
};

/**
 * \brief A source that has a distribution
 */
class distribution_source : public source {
public:
  /**
   * \brief Capture the distribution into the given distribution
   */
  virtual void get(distribution &into) const = 0;
};

} // namespace cxxmetrics::state