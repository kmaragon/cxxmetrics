#pragma once

#include "../bits/atomic/ringbuf.hpp"
#include "source.hpp"

namespace cxxmetrics::state {

/**
 * \brief A type of reservoir that simply keep the Size most recent values
 */
template <typename T, size_t Size>
class simple_atomic_distribution : public distribution<T> {
  atomic::ringbuf<T, Size> data_;

public:
  using value_type = T;

  simple_atomic_distribution() noexcept = default;

  simple_atomic_distribution(const simple_atomic_distribution &other) noexcept =
      default;

  ~simple_atomic_distribution() = default;

  simple_atomic_distribution &
  operator=(const simple_atomic_distribution &r) noexcept = default;

  void update(const T &v) noexcept override;

  void get(cxxmetrics::distribution &into) const override;

  [[nodiscard]] bool is_atomic() const noexcept override { return true; }
};

template <typename T, size_t Size>
void simple_atomic_distribution<T, Size>::get(
    cxxmetrics::distribution &into) const {
  auto it = data_.begin();
  auto sz = data_.size();
  into.reserve(sz);

  for (; it != data_.end(); ++it) {
    if (sz-- == 0)
      break;

    into.emplace_back(*it);
  }
}

template <typename T, size_t Size>
void simple_atomic_distribution<T, Size>::update(const T &v) noexcept {
  data_.push(v);
}

} // namespace cxxmetrics::state
