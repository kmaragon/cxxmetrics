#pragma once

#include "../bits/time.hpp"
#include "source.hpp"
#include <atomic>
#include <cmath>

namespace cxxmetrics::state {

namespace detail {

template <typename T>
struct atomic_adder {
  void operator()(std::atomic<T> &a, const T &b) const {
    a.fetch_add(b, std::memory_order_relaxed);
  }
};

template <typename T>
struct manual_atomic_adder {
  void operator()(std::atomic<T> &a, const T &b) const {
    while (true) {
      T v1 = a.load();
      T v2 = v1 + b;

      if (a.compare_exchange_weak(
              v1, v2, std::memory_order_relaxed, std::memory_order_relaxed))
        break;
    }
  }
};

template <>
struct atomic_adder<float> : public manual_atomic_adder<float> {};
template <>
struct atomic_adder<double> : public manual_atomic_adder<double> {};
template <>
struct atomic_adder<long double> : public manual_atomic_adder<long double> {};

template <typename TA, typename TB>
void atomic_add(std::atomic<TA> &a, const TB &b) {
  atomic_adder<TA> add;
  add(a, b);
}

template <typename ClockGet>
class clock_traits {
  static auto clk_point_() {
    ClockGet *clk;
    return (*clk)();
  }

  static auto clk_diff_() { return clk_point_() - clk_point_(); }

public:
  using clock_point = typename std::decay<decltype(clk_point_())>::type;
  using clock_diff = typename std::decay<decltype(clk_diff_())>::type;
};

template <typename ClockGet = steady_clock_point, typename T = double>
class atomic_ewma : public value_source {
  static_assert(
      std::is_arithmetic<T>::value,
      "ewma can only be applied to integral and floating point values");

public:
  using clock_point = typename clock_traits<ClockGet>::clock_point;
  using clock_diff = typename clock_traits<ClockGet>::clock_diff;

private:
  static long double alpha_;

  ClockGet clk_;
  std::atomic<T> rate_;
  clock_point last_;
  clock_diff window_;
  clock_diff interval_;
  std::atomic<T> pending_;
  std::atomic<bool> ticked_;

  static constexpr double get_alpha() {
    return 1 - exp((interval_.count() * -1.0l) / (window_.count() * 2.0l));
  }

  template <bool Write = true>
  T tick(const clock_point &at) noexcept;

public:
  atomic_ewma(
      const clock_diff &window,
      const clock_diff &interval,
      const ClockGet &clock = ClockGet{}) noexcept;
  atomic_ewma(const atomic_ewma &e) noexcept;
  ~atomic_ewma() = default;

  template <typename TAmt>
  void mark(TAmt amount) noexcept;

  bool compare_exchange(T &expectedrate, T rate) noexcept;

  T rate() noexcept;

  T rate() const noexcept;

  atomic_ewma &operator=(const atomic_ewma<ClockGet, T> &c) noexcept;
};

template <typename ClockGet, typename T>
long double atomic_ewma<ClockGet, T>::alpha_ = get_alpha();

template <typename ClockGet, typename T>
atomic_ewma<ClockGet, T>::atomic_ewma(
    const clock_diff &window,
    const clock_diff &interval,
    const ClockGet &clock) noexcept
    : clk_(clock), rate_(0), last_(clk_()), window_(window),
      interval_(interval), pending_(0), ticked_(false) {}

template <typename ClockGet, typename T>
atomic_ewma<ClockGet, T>::atomic_ewma(const atomic_ewma &c) noexcept
    : clk_(c.clk_), rate_(c.rate_.load()), last_(c.last_),
      pending_(c.pending_.load()), ticked_(c.ticked_.load()) {}

template <typename ClockGet, typename T>
template <typename Mark>
void atomic_ewma<ClockGet, T>::mark(Mark amount) noexcept {
  auto now = clk_();

  // our clock went backwards
  if (now < last_)
    return;

  tick(now);
  atomic_add(pending_, amount);
}

template <typename ClockGet, typename T>
bool atomic_ewma<ClockGet, T>::compare_exchange(T &expectedrate, T rate) noexcept {
  return rate_.compare_exchange_weak(
      expectedrate, rate, std::memory_order_relaxed, std::memory_order_relaxed);
}

template <typename ClockGet, typename T>
T atomic_ewma<ClockGet, T>::rate() noexcept {
  auto now = clk_();
  return tick(now);
}

template <typename ClockGet, typename T>
T atomic_ewma<ClockGet, T>::rate() const noexcept {
  auto now = clk_();
  // the const_cast is safe with the false template parameter
  return const_cast<atomic_ewma *>(this)->tick<false>(now);
}

template <typename ClockGet, typename T>
template <bool Write>
T atomic_ewma<ClockGet, T>::tick(const clock_point &at) noexcept {
  int missed_intervals;
  clock_point last;

  last = last_;

  auto pending = pending_.load();
  auto nrate = rate_.load();

  if (at < last)
    return nrate;

  if (nrate == T{} && !ticked_.load()) {
    if ((at - last) < interval_)
      return pending;

    bool ticked = false;
    if (ticked_.compare_exchange_weak(
            ticked,
            true,
            std::memory_order_relaxed,
            std::memory_order_relaxed)) {
      // use constexpr if with 17
      if (Write) {
        // one thread sets the last timestamp
        if (!pending_.compare_exchange_weak(
                pending,
                0,
                std::memory_order_relaxed,
                std::memory_order_relaxed))
          return pending; // someone else ticked from under us

        if (rate_.compare_exchange_weak(
                nrate,
                pending,
                std::memory_order_relaxed,
                std::memory_order_relaxed))
          last_ = at;
      }

      return pending;
    }
  }

  // apply the pending value to our current rate
  // if someone else already snagged the pending value, start over
  auto rate = nrate + (alpha_ * (pending - nrate));

  // figure out how many intervals we've missed
  missed_intervals = ((at - last) / interval_) - 1;
  if (missed_intervals > 0) {
    // we missed some intervals - we'll average in zeros
    if ((window_ > interval_) && (at - last) > window_) {
      constexpr auto intervals_per_window = window_ / (interval_ * 1.0);
      period::value missed_windows = missed_intervals / intervals_per_window;
      rate = pow(rate, 1 / pow(missed_windows, 2));

      // figure out the missed intervals now
      missed_intervals -= (missed_windows * intervals_per_window);
    }

    // this is the fastest way to do this - the alternative is a standard
    // repeated integral but this is faster
    for (int i = 0; i < missed_intervals; i++)
      rate = rate + (alpha_ * -rate);
  }

  if (std::isnan(rate) || std::isinf(rate))
    rate = T{};

  // make sure that last_ didn't catch up with us
  if (!Write || (at - last) < interval_)
    return rate;

  if (!pending_.compare_exchange_weak(
          pending, 0, std::memory_order_relaxed, std::memory_order_relaxed))
    return rate; // someone else already either ticked or added a pending value

  rate_.store(rate);
  if (last_ < at)
    last_ = at;

  return rate;
}

template <typename ClockGet, typename T>
atomic_ewma<ClockGet, T> &atomic_ewma<ClockGet, T>::operator=(const atomic_ewma &c) noexcept {
  alpha_ = c.alpha_;
  rate_.store(c.rate_.load());
  pending_.store(c.pending_.load());
  last_ = c.last_;
  window_ = c.window_;
  interval_ = c.interval_;
  return *this;
}

} // namespace detail

/**
 * \brief An exponential weighted moving average metric
 */
template <typename T = double>
class atomic_ewma : public value_source {
  detail::atomic_ewma<steady_clock_point, T> ewma_;

public:
  /**
   * \brief Construct an exponential weighted moving average
   *
   * \param window The window over which the average accounts for
   * \param interval The interval at which the average is calculated
   */
  atomic_ewma() noexcept = default;
  atomic_ewma(const atomic_ewma &ewma) noexcept = default;

  atomic_ewma &operator=(const atomic_ewma &e) noexcept = default;

  /**
   * \brief Mark the value in the ewma
   *
   * \param value the value to mark in the ewma
   */
  template <typename Mark>
  typename std::enable_if<std::is_arithmetic<Mark>::value, void>::type
  mark(Mark value) noexcept {
    ewma_.mark(value);
  }

  /**
   * \brief Get the current rate in the ewma
   *
   * \return the rate of the ewma
   */
  T rate() const noexcept { return ewma_.rate(); }

  /**
   * \brief Get the current rate in the ewma
   *
   * \return The rate of the ewma
   */
  T rate() noexcept { return ewma_.rate(); }

  /**
   * \brief Convenience operator to mark a value in the moving average
   *
   * \param value the amount to mark
   * \return a reference to the ewma
   */
  template <typename Mark>
  typename std::enable_if<std::is_arithmetic<Mark>::value, atomic_ewma<T>>::type &
  operator+=(
      typename std::enable_if<std::is_arithmetic<Mark>::value, Mark>::type
          value) noexcept {
    mark(value);
    return *this;
  }

  /**
   * Get a snapshot of the moving average
   */
  [[nodiscard]] value get() const noexcept override { return ewma_.rate(); }

  bool is_atomic() const noexcept override { return true; }
};

} // namespace cxxmetrics::state
