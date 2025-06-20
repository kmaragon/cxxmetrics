#pragma once

#include "../bits/atomic/ringbuf.hpp"
#include "distribution.hpp"

namespace cxxmetrics::state {

namespace detail {

template <typename T, typename ClockGet>
class timed_data {

public:
  using clock_point = typename clock_traits<ClockGet>::clock_point;

  timed_data() noexcept;

  timed_data(const ClockGet &t) noexcept;

  timed_data(const T &val, const ClockGet &t = ClockGet{}) noexcept;

  timed_data(const timed_data &) noexcept = default;

  timed_data &operator=(const timed_data &) noexcept = default;

  bool operator<(const timed_data &other) const noexcept;

  bool operator<=(const timed_data &other) const noexcept;

  bool operator>(const timed_data &other) const noexcept;

  bool operator>=(const timed_data &other) const noexcept;

  bool operator==(const timed_data &other) const noexcept;

  bool operator!=(const timed_data &other) const noexcept;

  clock_point time() const { return time_; }

  T value() const { return value_; }

private:
  clock_point time_;
  T value_;
};

template <typename T, typename ClockGet>
timed_data<T, ClockGet>::timed_data() noexcept : time_{}, value_{} {
  // invalid state constructor
}

template <typename T, typename ClockGet>
timed_data<T, ClockGet>::timed_data(const ClockGet &clock) noexcept
    : time_(clock()), value_{} {}

template <typename T, typename ClockGet>
timed_data<T, ClockGet>::timed_data(const T &val,
                                    const ClockGet &clock) noexcept
    : time_(clock()), value_(val) {}

template <typename T, typename ClockGet>
bool timed_data<T, ClockGet>::operator<(
    const timed_data &other) const noexcept {
  if (value_ < other.value_)
    return true;
  if (value_ == other.value_)
    return time_ < other.time_;
  return false;
};

template <typename T, typename ClockGet>
bool timed_data<T, ClockGet>::operator<=(
    const timed_data &other) const noexcept {
  if (value_ < other.value_)
    return true;
  if (value_ == other.value_)
    return time_ <= other.time_;
  return false;
};

template <typename T, typename ClockGet>
bool timed_data<T, ClockGet>::operator>(
    const timed_data &other) const noexcept {
  if (value_ > other.value_)
    return true;
  if (value_ == other.value_)
    return time_ > other.time_;
  return false;
};

template <typename T, typename ClockGet>
bool timed_data<T, ClockGet>::operator>=(
    const timed_data &other) const noexcept {
  if (value_ > other.value_)
    return true;
  if (value_ == other.value_)
    return time_ >= other.time_;
  return false;
};

template <typename T, typename ClockGet>
bool timed_data<T, ClockGet>::operator==(
    const timed_data &other) const noexcept {
  return value_ == other.value_ && time_ == other.time_;
};

template <typename T, typename ClockGet>
bool timed_data<T, ClockGet>::operator!=(
    const timed_data &other) const noexcept {
  return value_ != other.value_ || time_ != other.time_;
};

} // namespace detail

/**
 * \brief A reservoir implementation that manages samples based on activity from
 * the most recent time period
 *
 * \tparam T the type of element in the reservoir
 * \tparam Size the maximum size of the data in the reservoir
 * \tparam ClockGet the 'functor' (the C++ kind, not an actual functor) that
 * gets the current time
 */
template <typename T, size_t Size, typename ClockGet = steady_clock_point>
class atomic_sliding_window_distribution : public state::distribution<T> {
public:
  /**
   * \brief the type of data that the sliding window is represented in based on
   * the clock 'functor'
   */
  using window_type = typename detail::clock_traits<ClockGet>::clock_diff;
  using value_type = T;

private:
  class transform_and_filter_iterator
      : public std::iterator<T, std::input_iterator_tag> {
    using clock_point = typename detail::clock_traits<ClockGet>::clock_point;

    clock_point min_;
    typename detail::ringbuf<detail::timed_data<T, ClockGet>, Size>::iterator
        it_;
    typename detail::ringbuf<detail::timed_data<T, ClockGet>, Size>::iterator
        end_;

  public:
    transform_and_filter_iterator() : min_{} {}

    transform_and_filter_iterator(
        const clock_point &min,
        const typename detail::ringbuf<detail::timed_data<T, ClockGet>,
                                       Size>::iterator &real,
        const typename detail::ringbuf<detail::timed_data<T, ClockGet>,
                                       Size>::iterator &end) noexcept
        : min_(min), it_(real), end_(end) {
      while (it_ != end_ && it_->time() < min_)
        ++it_;
    }

    bool operator==(const transform_and_filter_iterator &other) const noexcept {
      return it_ == other.it_;
    }

    bool operator!=(const transform_and_filter_iterator &other) const noexcept {
      return it_ != other.it_;
    }

    transform_and_filter_iterator &operator++() noexcept {
      if (it_ == end_)
        return *this;

      ++it_;
      while (it_ != end_ && it_->time() < min_)
        ++it_;
      return *this;
    }

    T operator*() const noexcept { return it_->value(); }
  };

  ClockGet clock_;
  window_type window_;
  detail::ringbuf<detail::timed_data<T, ClockGet>, Size> data_;

public:
  /**
   * \brief Construct a sliding window reservoir
   *
   * \param window the size of the sliding window over which the reservoir
   * tracks
   * \param clock the clock object to use for deriving timestamps
   */
  explicit atomic_sliding_window_distribution(
      const window_type &window = time::minutes(1),
      const ClockGet &clock = ClockGet()) noexcept;

  /**
   * \brief Copy constructor
   */
  atomic_sliding_window_distribution(
      const atomic_sliding_window_distribution &other) noexcept;

  ~atomic_sliding_window_distribution() = default;

  /**
   * \brief Assignment operator
   */
  atomic_sliding_window_distribution &
  operator=(const atomic_sliding_window_distribution &other) noexcept;

  void update(const T &v) noexcept override;

  void get(cxxmetrics::distribution &into) const override;

  [[nodiscard]] bool is_atomic() const noexcept override { return true; }
};

template <typename T, size_t Size, typename ClockGet>
atomic_sliding_window_distribution<T, Size, ClockGet>::
    atomic_sliding_window_distribution(const window_type &window,
                                       const ClockGet &clock) noexcept
    : clock_(clock), window_(window) {}

template <typename T, size_t Size, typename ClockGet>
atomic_sliding_window_distribution<T, Size, ClockGet>::
    atomic_sliding_window_distribution(
        const atomic_sliding_window_distribution &other) noexcept
    : clock_(other.clock_), window_(other.window_), data_(other.data_) {}

template <typename T, size_t Size, typename ClockGet>
atomic_sliding_window_distribution<T, Size, ClockGet> &
atomic_sliding_window_distribution<T, Size, ClockGet>::operator=(
    const atomic_sliding_window_distribution &other) noexcept {
  data_ = other.data_;
  clock_ = other.clock_;
  window_ = other.window_;
}

template <typename T, size_t Size, typename ClockGet>
void atomic_sliding_window_distribution<T, Size, ClockGet>::update(
    const T &v) noexcept {
  // set up our values for trimming old stuff out
  data_.push(detail::timed_data<T, ClockGet>(v, clock_));
}

template <typename T, size_t Size, typename ClockGet>
void atomic_sliding_window_distribution<T, Size, ClockGet>::get(
    cxxmetrics::distribution &into) const noexcept {
  auto now = clock_();
  auto min = now - window_;

  auto start = data_.begin();
  auto end = data_.end();

  auto tstart = transform_and_filter_iterator(min, start, end);
  auto tend = transform_and_filter_iterator(min, end, end);

  auto sz = data_.size();
  into.clear();
  into.reserve(sz);

  for (; tstart != tend; ++tstart) {
    if (sz-- == 0) {
      break;
    }
    into.emplace_back(*tstart);
  }
}

} // namespace cxxmetrics::state
