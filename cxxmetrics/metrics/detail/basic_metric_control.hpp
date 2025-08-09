#pragma once

#include "../composite_metric.hpp"
#include "../../detail/concurrent/set.hpp"

#include <atomic>

namespace cxxmetrics::metrics::detail {

class metric_container* container_;

/**
 * \brief The control portion of a native metric smart pointer.
 * 
 * The virtual methods allow for supporting alternate storage schemes. For example, a basic_metric_control
 * may wrap a standard shared_ptr. The "standard" implemetation will be very much like the standard shared_ptr.
 */
class basic_metric_control {
public:
  basic_metric_control(const basic_metric_control&);
  basic_metric_control(basic_metric_control&&) noexcept;
  basic_metric_control& operator=(const basic_metric_control&);
  basic_metric_control& operator=(basic_metric_control&&) noexcept;
  ~basic_metric_control() noexcept;

  /**
   * \brief Check if the control is controlling a metric
   */
  operator bool() const noexcept { return metric() != nullptr; }

  /**
   * \brief Get the metric that the control owns and is controlling
   */
  virtual composite_metric* metric() const;

private:
  // tracks which containers the metric is registered in. If the handles all
  // go away, This is used to invalidate the metric in the containers. That
  // process does not guarantee that the metric will be deleted or all
  // references removed. But it at least makes it possible.
  cxxmetrics::detail::concurrent::set<metric_container*> containers_;

  // the references that handles have. When this goes to 0, the metric should
  // be automatically deregistered from its containers
  std::atomic_size_t handle_references_;

  // the references that containers have. When this goes to 0, the metric is
  // no longer registered in any containers and any handles that are there
  // are no longer useful. Except that they can be used to register the metric
  // in a new container so we keep it alive
  std::atomic_size_t container_references_;
};

} // namespace cxxmetrics::metrics