#pragma once

namespace cxxmetrics::metrics::detail {

/**
 * \brief A handle to a type of metric state that tracks references
 */
template <typename T>
class metric_handle {
  // handle needs to support a shared_ptr
  // and also a custom version that shares state with registries that automatically
  // deregisters when there are no handles left
public:
  T* operator->() const;
};

}