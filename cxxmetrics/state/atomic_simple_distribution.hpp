#pragma once

#include "../bits/atomic/ringbuf.hpp"
#include "distribution.hpp"

namespace cxxmetrics::state {

/**
 * \brief A type of reservoir that simply keep the Size most recent values
 */
template <typename T, size_t Size>
class atomic_simple_distribution : public state::distribution<T> {
  atomic::ringbuf<T, Size> data_;

public:
  using value_type = T;

  atomic_simple_distribution() noexcept = default;

  atomic_simple_distribution(const atomic_simple_distribution &other) noexcept =
      default;

  ~atomic_simple_distribution() = default;

  atomic_simple_distribution &
  operator=(const atomic_simple_distribution &r) noexcept = default;

  void update(const T &v) noexcept override;

  void get(cxxmetrics::distribution &into) const override;

  [[nodiscard]] bool is_atomic() const noexcept override { return true; }
};

template <typename T, size_t Size>
void atomic_simple_distribution<T, Size>::get(
    cxxmetrics::distribution &into) const {
  auto it = data_.begin();
  auto sz = data_.size();
  into.clear();
  into.reserve(sz);

  for (; it != data_.end(); ++it) {
    if (sz-- == 0)
      break;

    into.emplace_back(*it);
  }
}

template <typename T, size_t Size>
void atomic_simple_distribution<T, Size>::update(const T &v) noexcept {
  data_.push(v);
}

} // namespace cxxmetrics::state
