#pragma once

#include "distribution.hpp"
#include <atomic>
#include <chrono>
#include <random>

namespace cxxmetrics::state {

/**
 * \brief a Uniform Reservoir for getting percentile samples
 *
 * \tparam T the type of elements in the reservoir
 * \tparam Size the size of the reservoir
 */
template <typename T, std::size_t Size>
class atomic_uniform_distribution : public state::distribution<T> {
  std::default_random_engine gen_;
  std::array<std::atomic<T>, Size> elems_;
  std::atomic<std::size_t> count_;

  static unsigned generate_seed() noexcept {
    auto full = std::chrono::system_clock::now().time_since_epoch();
    auto nano = std::chrono::duration_cast<std::chrono::nanoseconds>(full);

    auto seed = nano.count();
    size_t size = sizeof(seed);
    while (size > sizeof(unsigned)) {
      seed = (seed & 0xffffffff) ^ (seed >> 32);
      size -= 4;
    }

    return seed;
  }

public:
  using value_type = T;

  /**
   * \brief Construct a uniform reservoir
   */
  atomic_uniform_distribution() noexcept;

  /**
   * \brief Copy constructor
   */
  atomic_uniform_distribution(const atomic_uniform_distribution &r) noexcept;
  ~atomic_uniform_distribution() = default;

  /**
   * \brief Assignment operator
   */
  atomic_uniform_distribution &
  operator=(const atomic_uniform_distribution &other) noexcept;

  void update(const T &v) noexcept override;

  void get(cxxmetrics::distribution &into) const override;

  [[nodiscard]] bool is_atomic() const noexcept override { return true; }
};

template <typename T, std::size_t Size>
atomic_uniform_distribution<T, Size>::atomic_uniform_distribution() noexcept
    : gen_(generate_seed()), count_(0) {}

template <typename T, std::size_t Size>
atomic_uniform_distribution<T, Size>::atomic_uniform_distribution(
    const atomic_uniform_distribution &other) noexcept
    : gen_(generate_seed()),
      count_(other.count_.exchange(0, std::memory_order_relaxed)) {
  for (std::size_t i = 0; i < count_; i++)
    elems_[i].store(other.elems_[i].exchange(0, std::memory_order_relaxed),
                    std::memory_order_relaxed);
}

template <typename T, std::size_t Size>
atomic_uniform_distribution<T, Size> &
atomic_uniform_distribution<T, Size>::operator=(
    const atomic_uniform_distribution &other) noexcept {
  for (std::size_t i = 0; i < Size; i++)
    elems_[i].store(other.elems_[i].load(std::memory_order_relaxed),
                    std::memory_order_relaxed);
  count_.store(other.count_.load(std::memory_order_relaxed),
               std::memory_order_relaxed);
  return *this;
}

template <typename T, std::size_t Size>
void atomic_uniform_distribution<T, Size>::update(const T &value) noexcept {
  auto c = count_.fetch_add(1, std::memory_order_relaxed);

  if (c < Size) {
    elems_[c].store(value, std::memory_order_relaxed);
    return;
  }

  // so we don't run out of count
  count_.store(Size, std::memory_order_relaxed);

  std::uniform_int_distribution<> d(0, Size);
  elems_[d(gen_)].store(value, std::memory_order_relaxed);
}

template <typename T, std::size_t Size>
void atomic_uniform_distribution<T, Size>::get(
    cxxmetrics::distribution &into) const {
  auto sz = count_.load(std::memory_order_relaxed);
  if (sz > Size) {
    sz = Size;
  }

  into.clear();
  into.reserve(sz);

  for (std::size_t i = 0; i < sz; i++) {
    into.emplace_back(elems_[i].load(std::memory_order_relaxed), sz);
  }
}

} // namespace cxxmetrics::state
