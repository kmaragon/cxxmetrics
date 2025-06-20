#pragma once

#include <fmt/format.h>
#include <charconv>
#include <chrono>
#include <string>
#include <string_view>
#include <variant>

namespace cxxmetrics {

namespace detail {

class variant_data {
public:
  virtual ~variant_data() = default;
  virtual void serialize(fmt::memory_buffer &onto) const = 0;
  virtual long long to_integral(bool *valid) const = 0;
  virtual long double to_float(bool *valid) const = 0;
  virtual std::chrono::nanoseconds to_nanos(bool *valid) const = 0;
  virtual int type_score() const = 0;
  virtual std::size_t hash_value() const noexcept = 0;
  virtual void copy(void *into) const noexcept = 0;
  virtual void move(void *into) noexcept { return copy(into); }
  virtual void add(const variant_data &other) noexcept = 0;
  virtual void multiply(const variant_data &other) noexcept = 0;
  virtual void divide(const variant_data &other) noexcept = 0;
  virtual void negate() noexcept = 0;
  virtual void bitwise_negate() noexcept = 0;
  virtual int compare(const variant_data &other) const noexcept = 0;
};

template <typename T> class integral_variant_data : public variant_data {
  static_assert(std::is_integral<T>::value,
                "Expected an integral type for integral variant data");
  T val_;

public:
  integral_variant_data(T value) : val_(value) {}

  void serialize(fmt::memory_buffer &onto) const override {
    fmt::format_to(std::back_inserter(onto), "{}", val_);
  }

  long long to_integral(bool *valid) const override {
    if (valid)
      *valid = true;
    return static_cast<long long>(val_);
  }

  long double to_float(bool *valid) const override {
    if (valid)
      *valid = true;
    return static_cast<long double>(val_);
  }

  std::chrono::nanoseconds to_nanos(bool *valid) const override {
    return std::chrono::nanoseconds(to_integral(valid));
  }

  std::size_t hash_value() const noexcept override { return std::hash<T>()(val_); }

  void copy(void *into) const noexcept override {
    new (reinterpret_cast<integral_variant_data<T> *>(into))
        integral_variant_data<T>(val_);
  }

  int type_score() const override {
    return (sizeof(T) * 10) + (std::is_signed<T>::value ? 0 : 1);
  }

  void add(const variant_data &other) noexcept override {
    bool valid;
    auto lv = other.to_integral(&valid);
    if (valid)
      val_ += lv;
    else
      val_ += other.to_float(nullptr);
  }

  void multiply(const variant_data &other) noexcept override {
    bool valid;
    auto lv = other.to_integral(&valid);
    if (valid) {
      val_ *= lv;
    } else {
      auto fv = other.to_float(&valid);
      if (valid)
        val_ *= fv;
    }
  }

  void divide(const variant_data &other) noexcept override {
    bool valid;
    auto lv = other.to_integral(&valid);
    if (valid) {
      if (!lv)
        val_ = 0;
      else
        val_ /= lv;
    } else {
      auto fv = other.to_float(&valid);
      if (valid) {
        if (!fv)
          val_ = 0;
        else
          val_ /= fv;
      }
    }
  }

  void negate() noexcept override { val_ = -val_; }

  void bitwise_negate() noexcept override { val_ = ~val_; }

  int compare(const variant_data &other) const noexcept override {
    bool valid;
    auto lv = static_cast<T>(other.to_integral(&valid));
    if (!valid) {
      auto fv = other.to_float(&valid);
      if (!valid)
        return -1;

      return val_ < fv ? -1 : val_ == fv ? 0 : 1;
    }

    return val_ - lv;
  }
};

template <typename T> class float_variant_data : public variant_data {
  static_assert(std::is_floating_point<T>::value,
                "Expected an integral type for integral variant data");
  T val_;

public:
  float_variant_data(T value) : val_(value) {}

  void serialize(fmt::memory_buffer &onto) const override {
    fmt::format_to(std::back_inserter(onto), "{}", val_);
  }

  long long to_integral(bool *valid) const override {
    if (valid)
      *valid = true;
    return static_cast<long long>(std::round(val_));
  }

  long double to_float(bool *valid) const override {
    if (valid)
      *valid = true;
    return static_cast<long double>(val_);
  }

  std::chrono::nanoseconds to_nanos(bool *valid) const override {
    if (valid)
      *valid = true;
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(1) * val_);
  }

  std::size_t hash_value() const noexcept override { return std::hash<T>()(val_); }

  void copy(void *into) const noexcept override {
    new (reinterpret_cast<float_variant_data<T> *>(into))
        float_variant_data<T>(val_);
  }

  int type_score() const override { return (sizeof(T) * 20); }

  void add(const variant_data &other) noexcept override {
    val_ += other.to_float(nullptr);
  }

  void multiply(const variant_data &other) noexcept override {
    bool valid;
    auto fv = other.to_float(&valid);
    if (valid)
      val_ *= fv;
  }

  void divide(const variant_data &other) noexcept override {
    bool valid;
    auto fv = other.to_float(&valid);
    if (valid) {
      if (!fv)
        val_ = 0;
      else
        val_ /= fv;
    }
  }

  void negate() noexcept override { val_ = -val_; }

  void bitwise_negate() noexcept override {}

  int compare(const variant_data &other) const noexcept override {
    bool valid;
    auto fv = static_cast<T>(other.to_float(&valid));
    if (!valid)
      return -1;

    return val_ < fv ? -1 : val_ == fv ? 0 : 1;
  }
};

class string_variant_data : public variant_data {
  std::string val_;

public:
  string_variant_data(std::string value);
  void serialize(fmt::memory_buffer &onto) const override;
  long long to_integral(bool *valid) const override;
  long double to_float(bool *valid) const override;
  std::chrono::nanoseconds to_nanos(bool *valid) const override;
  std::size_t hash_value() const noexcept override;
  void copy(void *into) const noexcept override;
  void move(void *into) noexcept override;
  int type_score() const override;
  void add(const variant_data &other) noexcept override;
  void multiply(const variant_data &other) noexcept override;
  void divide(const variant_data &other) noexcept override;
  void negate() noexcept override;
  void bitwise_negate() noexcept override;
  int compare(const variant_data &other) const noexcept override;
};

class string_view_variant_data : public variant_data {
  std::string_view val_;

public:
  string_view_variant_data(const std::string_view& value);
  void serialize(fmt::memory_buffer &onto) const override;
  long long to_integral(bool *valid) const override;
  long double to_float(bool *valid) const override;
  std::chrono::nanoseconds to_nanos(bool *valid) const override;
  std::size_t hash_value() const noexcept override;
  void copy(void *into) const noexcept override;
  void move(void *into) noexcept override;
  int type_score() const override;
  void add(const variant_data &other) noexcept override;
  void multiply(const variant_data &other) noexcept override;
  void divide(const variant_data &other) noexcept override;
  void negate() noexcept override;
  void bitwise_negate() noexcept override;
  int compare(const variant_data &other) const noexcept override;
};

template <typename TRep, typename TPeriod>
class duration_variant_data : public variant_data {
  std::chrono::duration<TRep, TPeriod> dur_;

public:
  duration_variant_data(std::chrono::duration<TRep, TPeriod> value)
      : dur_(value) {}

  void serialize(fmt::memory_buffer& onto) const override {
    auto hours = std::chrono::duration_cast<std::chrono::hours>(dur_);
    auto minutes = std::chrono::duration_cast<std::chrono::minutes>(dur_ - hours);
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(dur_ - hours - minutes);
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(dur_ - hours - minutes - seconds);
    auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(dur_ - hours - minutes - seconds - milliseconds);
    auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(dur_ - hours - minutes - seconds - milliseconds - microseconds);

    auto ssz = onto.size();
    if (hours.count()) {
      fmt::format_to(std::back_inserter(onto), "{}h", hours.count());
    }
    if (minutes.count()) {
      fmt::format_to(std::back_inserter(onto), "{}m", minutes.count());
    }
    if (seconds.count()) {
      fmt::format_to(std::back_inserter(onto), "{}s", seconds.count());
    }
    if (milliseconds.count()) {
      fmt::format_to(std::back_inserter(onto), "{}ms", milliseconds.count());
    }
    if (microseconds.count()) {
      fmt::format_to(std::back_inserter(onto), "{}us", microseconds.count());
    }
    if (nanoseconds.count()) {
      fmt::format_to(std::back_inserter(onto), "{}ns", nanoseconds.count());
    }

    if (ssz == onto.size()) {
        onto.push_back('0');
    }
  }

  long long to_integral(bool *valid) const override {
    if (valid)
      *valid = true;
    return static_cast<long long>(dur_.count());
  }

  long double to_float(bool *valid) const override {
    if (valid)
      *valid = true;
    return static_cast<long double>(dur_.count());
  }

  std::chrono::nanoseconds to_nanos(bool *valid) const override {
    if (valid)
      *valid = true;
    return std::chrono::duration_cast<std::chrono::nanoseconds>(dur_);
  }

  std::size_t hash_value() const noexcept override {
    return std::hash<std::chrono::duration<TRep, TPeriod>>()(dur_);
  }

  void copy(void *into) const noexcept override {
    new (reinterpret_cast<duration_variant_data<TRep, TPeriod> *>(into))
        duration_variant_data<TRep, TPeriod>(dur_);
  }

  int type_score() const override {
    if (std::is_floating_point<TRep>::value)
      return (sizeof(TRep) * 20) + 2;
    else
      return (sizeof(TRep) * 10) + 2;
  }

  void add(const variant_data &other) noexcept override {
    dur_ += std::chrono::duration_cast<std::chrono::duration<TRep, TPeriod>>(
        other.to_nanos(nullptr));
  }

  void multiply(const variant_data &other) noexcept override {
    bool valid;
    auto lv = other.to_integral(&valid);
    if (!valid) {
      auto fv = other.to_float(&valid);
      if (!valid)
        return;

      dur_ *= fv;
      return;
    }

    dur_ *= lv;
  }

  void divide(const variant_data &other) noexcept override {
    bool valid;
    auto lv = other.to_integral(&valid);
    if (!valid) {
      auto fv = other.to_float(&valid);
      if (!valid)
        return;

      if (!fv)
        dur_ = dur_.zero();
      else
        dur_ /= fv;

      return;
    }

    if (!lv)
      dur_ = dur_.zero();
    else
      dur_ /= lv;
  }

  void negate() noexcept override { dur_ = -dur_; }

  void bitwise_negate() noexcept override {}

  int compare(const variant_data &other) const noexcept override {
    bool valid;
    auto n = other.to_nanos(&valid);
    return dur_ < n ? -1 : dur_ == n ? 0 : 1;
  }
};

class variant_data_holder {
  std::aligned_union<
      sizeof(std::max_align_t), integral_variant_data<long long>,
      float_variant_data<long double>, string_variant_data, string_view_variant_data,
      duration_variant_data<long long,
                            typename std::chrono::nanoseconds::period>>::type v;

  template <typename T> T *as() {
    return reinterpret_cast<T *>(std::addressof(v));
  }

  template <typename T> const T *as() const {
    return reinterpret_cast<const T *>(std::addressof(v));
  }

  variant_data_holder(const variant_data *cp);
  variant_data_holder uc_add(const variant_data_holder &other) const;
  variant_data_holder uc_mult(const variant_data_holder &other) const;
  variant_data_holder uc_div(const variant_data_holder &other) const;

  template <typename T, typename> struct float_int_initializer;

  template <typename _T>
  struct float_int_initializer<
      _T, typename std::enable_if<std::is_integral<_T>::value, void>::type> {
    void operator()(void *data, _T value) const {
      new (data) integral_variant_data<_T>(value);
    }
  };

  template <typename _T>
  struct float_int_initializer<
      _T,
      typename std::enable_if<std::is_floating_point<_T>::value, void>::type> {
    void operator()(void *data, _T value) const {
      new (data) float_variant_data<_T>(value);
    }
  };

public:
  template <typename TInt, typename = typename std::enable_if<
                               std::is_integral<TInt>::value ||
                                   std::is_floating_point<TInt>::value,
                               void>::type>
  variant_data_holder(TInt value) {
    float_int_initializer<TInt, void> init;
    init(as<void>(), value);
  }

  variant_data_holder(std::string value);
  variant_data_holder(const char *value);
  variant_data_holder(const variant_data_holder &from);
  variant_data_holder(variant_data_holder &&from);

  template <typename TRep, typename TPeriod>
  variant_data_holder(std::chrono::duration<TRep, TPeriod> value) {
    new (as<duration_variant_data<TRep, TPeriod>>())
        duration_variant_data<TRep, TPeriod>(value);
  }

  variant_data_holder negate() const;
  variant_data_holder bitwise_negate() const;
  ~variant_data_holder();
  variant_data_holder &operator=(variant_data_holder &&other) noexcept;
  long long to_integral() const;
  void serialize(fmt::memory_buffer& onto) const;
  long double to_float() const;
  std::chrono::nanoseconds to_nanos() const;
  int compare(const variant_data_holder &other) const;
  variant_data_holder add(const variant_data_holder &other) const;
  variant_data_holder multiply(const variant_data_holder &other) const;
  variant_data_holder divide(const variant_data_holder &other) const;
  std::size_t hash_value() const noexcept;
};

} // namespace detail

/**
 * \brief A generic value that is produced from some metric
 */
class value {
public:
  template <typename TInt, typename = typename std::enable_if<
                               std::is_integral<TInt>::value ||
                                   std::is_floating_point<TInt>::value,
                               void>::type>
  value(TInt value) noexcept : value_(value) {}

  value(std::string value) noexcept : value_(std::move(value)) {}

  value(const char *value) : value_(value) {}

  template <typename TRep, typename TPeriod>
  value(std::chrono::duration<TRep, TPeriod> value) : value_(value) {}

  value(const value &other);
  value(value &&other);
  value &operator=(value &&other) noexcept;
  value &operator+=(const value &other);
  value &operator-=(const value &other);
  value &operator/=(const value &other);
  value &operator*=(const value &other);
  value operator+(const value &other) const;
  value operator-(const value &other) const;
  value operator/(const value &other) const;
  value operator*(const value &other) const;
  value operator-() const;
  value operator~() const;
  bool operator>(const value &other) const;
  bool operator>=(const value &other) const;
  bool operator<(const value &other) const;
  bool operator<=(const value &other) const;
  bool operator==(const value &other) const;
  bool operator!=(const value &other) const;

#define metric_value_AUTOCAST_INTEGRAL(type)                                   \
  operator type() const { return static_cast<type>(value_.to_integral()); }

  metric_value_AUTOCAST_INTEGRAL(int8_t)
  metric_value_AUTOCAST_INTEGRAL(uint8_t)
  metric_value_AUTOCAST_INTEGRAL(int16_t)
  metric_value_AUTOCAST_INTEGRAL(uint16_t)
  metric_value_AUTOCAST_INTEGRAL(int32_t)
  metric_value_AUTOCAST_INTEGRAL(uint32_t)
  metric_value_AUTOCAST_INTEGRAL(int64_t)
  metric_value_AUTOCAST_INTEGRAL(uint64_t)

#undef metric_value_AUTOCAST_INTEGRAL
#define metric_value_AUTOCAST_FLOAT(type)                                      \
  operator type() const { return static_cast<type>(value_.to_float()); }

  metric_value_AUTOCAST_FLOAT(float)
  metric_value_AUTOCAST_FLOAT(double)
  metric_value_AUTOCAST_FLOAT(long double)

#undef metric_value_AUTOCAST_FLOAT

  operator std::string() const;

  template <typename TRep, typename TPer>
  operator std::chrono::duration<TRep, TPer>() const {
    return std::chrono::duration_cast<std::chrono::duration<TRep, TPer>>(
        value_.to_nanos());
  }

private:
  detail::variant_data_holder value_;

  value(detail::variant_data_holder &&nv);

  friend struct std::hash<value>;
  friend struct fmt::formatter<value>;
};

} // namespace cxxmetrics

namespace fmt {

template<>
struct formatter<cxxmetrics::value>
{
    template<typename ParseContext>
    constexpr auto parse(ParseContext& ctx) { return ctx.begin(); }

    template<typename FormatContext>
    auto format(const cxxmetrics::value& v, FormatContext& ctx) { 
        static thread_local fmt::memory_buffer buf;
        buf.clear();
        v.value_.serialize(buf);
        return format_to(ctx.out(), "{}", std::string_view(buf.data(), buf.size()));
     }
};


}

namespace std
{

template<>
struct hash<cxxmetrics::value>
{
    std::size_t operator()(const cxxmetrics::value &v) const noexcept
    {
        return v.value_.hash_value();
    }
};

namespace chrono
{

template<typename _ToDur>
constexpr _ToDur duration_cast(const cxxmetrics::value &v)
{
    return duration_cast<_ToDur>(static_cast<nanoseconds>(v));
}

}

template<typename Char, typename CharTraits>
basic_ostream<Char, CharTraits> &operator<<(basic_ostream<Char, CharTraits>& out, const cxxmetrics::value& val) {
  return out << static_cast<string>(val);
}

}