#ifndef CXXMETRICS_METRIC_VALUE_HPP
#define CXXMETRICS_METRIC_VALUE_HPP

#include <chrono>
#include <string>
#include <cmath>
#include <cstddef>

namespace cxxmetrics
{

namespace internal
{

class variant_data
{
public:
    virtual ~variant_data() = default;
    virtual std::string to_string() const = 0;
    virtual long long to_integral() const = 0;
    virtual long double to_float() const = 0;
    virtual std::chrono::nanoseconds to_nanos() const = 0;
    virtual std::size_t hash_value() const = 0;
    virtual bool operator==(const variant_data& other) const = 0;
    virtual void copy(void* into) const noexcept = 0;
    virtual void move(void* into) noexcept { return copy(into); }
};

template<typename T>
class integral_variant_data : public variant_data
{
    // can't do this without constexpr if
    // static_assert(std::is_integral<T>::value, "Expected an integral type for integral variant data");
    T val_;
public:
    integral_variant_data(T value) :
            val_(value)
    { }

    std::string to_string() const override
    {
        return std::to_string(val_);
    }

    long long to_integral() const override
    {
        return static_cast<long long>(val_);
    }

    long double to_float() const override
    {
        return static_cast<long double>(val_);
    }

    std::chrono::nanoseconds to_nanos() const override
    {
        return std::chrono::nanoseconds(to_integral());
    }

    std::size_t hash_value() const override
    {
        return std::hash<T>()(val_);
    }

    bool operator==(const variant_data& other) const override
    {
        return hash_value() == other.hash_value() && to_integral() == other.to_integral();
    }

    void copy(void* into) const noexcept override
    {
        new (reinterpret_cast<integral_variant_data<T>*>(into)) integral_variant_data<T>(val_);
    }
};

template<typename T>
class float_variant_data : public variant_data
{
    // can't do this without constexpr if
    // static_assert(std::is_floating_point<T>::value, "Expected an integral type for integral variant data");
    T val_;
public:
    float_variant_data(T value) :
            val_(value)
    { }

    virtual std::string to_string() const override
    {
        return std::to_string(val_);
    }

    virtual long long to_integral() const override
    {
        return static_cast<long long>(std::round(val_));
    }

    virtual long double to_float() const override
    {
        return static_cast<long double>(val_);
    }

    std::chrono::nanoseconds to_nanos() const override
    {
        return std::chrono::nanoseconds(to_integral());
    }

    std::size_t hash_value() const override
    {
        return std::hash<T>()(val_);
    }

    bool operator==(const variant_data& other) const override
    {
        return hash_value() == other.hash_value() && to_float() == other.to_float();
    }

    void copy(void* into) const noexcept override
    {
        new (reinterpret_cast<float_variant_data<T>*>(into)) float_variant_data<T>(val_);
    }
};

class string_variant_data : public variant_data
{
    std::string val_;
public:
    string_variant_data(std::string value) :
            val_(std::move(value))
    { }

    std::string to_string() const override
    {
        return val_;
    }

    long long to_integral() const override
    {
        if (!val_.length())
            return 0;

        char* end = nullptr;
        if (val_[0] == '-')
        {
            auto res = strtoll(val_.c_str(), &end, 10);
            if (!end || *end)
                return 0;
            return res;
        }

        auto res = strtoull(val_.c_str(), &end, 10);
        if (!end || *end)
            return 0;
        return res;
    }

    long double to_float() const override
    {
        if (!val_.length())
            return std::numeric_limits<long double>::quiet_NaN();

        char* end = nullptr;
        auto res = strtold(val_.c_str(), &end);
        if (!end || *end)
            return std::numeric_limits<long double>::quiet_NaN();
        return res;
    }

    std::chrono::nanoseconds to_nanos() const override
    {
        return std::chrono::nanoseconds(to_integral());
    }

    std::size_t hash_value() const override
    {
        return std::hash<std::string>()(val_);
    }

    bool operator==(const variant_data& other) const override
    {
        return hash_value() == other.hash_value() && val_ == other.to_string();
    }

    void copy(void* into) const noexcept override
    {
        new (reinterpret_cast<string_variant_data*>(into)) string_variant_data(val_);
    }

    void move(void* into) const
    {
        new (reinterpret_cast<string_variant_data*>(into)) string_variant_data(std::move(val_));
    }
};

template<typename TRep, typename TPeriod>
class duration_variant_data : public variant_data
{
    std::chrono::duration<TRep, TPeriod> dur_;
public:
    duration_variant_data(std::chrono::duration<TRep, TPeriod> value) :
            dur_(value)
    { }

    std::string to_string() const override
    {
        return std::to_string(dur_.count());
    }

    long long to_integral() const override
    {
        return static_cast<long long>(dur_.count());
    }

    long double to_float() const override
    {
        return static_cast<long double>(dur_.count());
    }

    std::chrono::nanoseconds to_nanos() const override
    {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(dur_);
    }

    std::size_t hash_value() const override
    {
        return std::hash<std::string>()(dur_);
    }

    bool operator==(const variant_data& other) const override
    {
        return hash_value() == other.hash_value() && to_nanos() == other.to_nanos();
    }

    void copy(void* into) const noexcept override
    {
        new (reinterpret_cast<duration_variant_data<TRep, TPeriod>*>(into)) duration_variant_data<TRep, TPeriod>(dur_);
    }
};

class variant_data_holder
{
    std::aligned_union<sizeof(std::max_align_t),
             integral_variant_data<long long>,
             float_variant_data<long double>,
             string_variant_data,
             duration_variant_data<long long, typename std::chrono::nanoseconds::period>
    >::type v;

    template<typename T>
    T* as()
    {
        return reinterpret_cast<T*>(std::addressof(v));
    }

    template<typename T>
    const T* as() const
    {
        return reinterpret_cast<const T*>(std::addressof(v));
    }
public:
    template<typename TInt, typename = typename std::enable_if<std::is_integral<TInt>::value || std::is_floating_point<TInt>::value, void>::type>
    variant_data_holder(TInt value)
    {
        if (std::is_floating_point<TInt>::value)
            new (as<float_variant_data<TInt>>()) float_variant_data<TInt>(value);
        else
            new (as<integral_variant_data<TInt>>()) integral_variant_data<TInt>(value);
    }

    variant_data_holder(std::string value)
    {
        new (as<string_variant_data>()) string_variant_data(std::move(value));
    }

    variant_data_holder(const char* value)
    {
        new (as<string_variant_data>()) string_variant_data(value);
    }

    variant_data_holder(const variant_data_holder& from)
    {
        from.as<variant_data>()->copy(std::addressof(v));
    }

    variant_data_holder(variant_data_holder&& from)
    {
        from.as<variant_data>()->move(std::addressof(v));
    }

    template<typename TRep, typename TPeriod>
    variant_data_holder(std::chrono::duration<TRep, TPeriod> value)
    {
        new (as<duration_variant_data<TRep, TPeriod>>()) duration_variant_data<TRep, TPeriod>(value);
    }

    ~variant_data_holder()
    {
        as<variant_data>()->~variant_data();
    }

    variant_data_holder& operator=(variant_data_holder&& other) noexcept
    {
        as<variant_data>()->~variant_data();
        other.as<variant_data>()->move(std::addressof(v));
        return *this;
    }

    auto to_integral() const
    {
        return as<variant_data>()->to_integral();
    }

    auto to_string() const
    {
        return as<variant_data>()->to_string();
    }

    auto to_float() const
    {
        return as<variant_data>()->to_float();
    }

    auto to_nanos() const
    {
        return as<variant_data>()->to_nanos();
    }

    auto hash_value() const
    {
        return as<variant_data>()->hash_value();
    }

    bool operator==(const variant_data_holder& other) const
    {
        return *as<variant_data>() == *other.as<variant_data>();
    }

    bool operator!=(const variant_data_holder& other) const
    {
        return !operator==(other);
    }
};

}

class metric_value
{
    internal::variant_data_holder value_;

    template<typename T>
    friend struct std::hash;
public:
    template<typename TInt, typename = typename std::enable_if<std::is_integral<TInt>::value || std::is_floating_point<TInt>::value, void>::type>
    metric_value(TInt value) noexcept :
            value_(value)
    { }

    metric_value(std::string value) noexcept :
            value_(std::move(value))
    { }

    metric_value(const char* value) :
            value_(value)
    { }

    template<typename TRep, typename TPeriod>
    metric_value(std::chrono::duration<TRep, TPeriod> value) :
            value_(value)
    { }

    metric_value(const metric_value& other) = default;
    metric_value(metric_value&& other) :
            value_(std::move(other))
    { }

    metric_value& operator=(metric_value&& other)
    {
        value_ = std::move(other.value_);
        return *this;
    }

    bool operator==(const metric_value& other) const
    {
        return value_ == other.value_;
    }

    bool operator!=(const metric_value& other) const
    {
        return value_ != other.value_;
    }

#define METRIC_VALUE_AUTOCAST_INTEGRAL(type) \
    operator type() const \
    { \
        return static_cast<type>(value_.to_integral()); \
    }

    METRIC_VALUE_AUTOCAST_INTEGRAL(int8_t)
    METRIC_VALUE_AUTOCAST_INTEGRAL(uint8_t)
    METRIC_VALUE_AUTOCAST_INTEGRAL(int16_t)
    METRIC_VALUE_AUTOCAST_INTEGRAL(uint16_t)
    METRIC_VALUE_AUTOCAST_INTEGRAL(int32_t)
    METRIC_VALUE_AUTOCAST_INTEGRAL(uint32_t)
    METRIC_VALUE_AUTOCAST_INTEGRAL(int64_t)
    METRIC_VALUE_AUTOCAST_INTEGRAL(uint64_t)

#undef METRIC_VALUE_AUTOCAST_INTEGRAL
#define METRIC_VALUE_AUTOCAST_FLOAT(type) \
    operator type() const \
    { \
        return static_cast<type>(value_.to_float()); \
    }

    METRIC_VALUE_AUTOCAST_FLOAT(float)
    METRIC_VALUE_AUTOCAST_FLOAT(double)
    METRIC_VALUE_AUTOCAST_FLOAT(long double)

#undef METRIC_VALUE_AUTOCAST_FLOAT

    operator std::string() const
    {
        return value_.to_string();
    }

    operator std::chrono::nanoseconds() const
    {
        return value_.to_nanos();
    }
};

inline std::ostream& operator<<(std::ostream& stream, const metric_value& ss) {
    return (stream << static_cast<std::string>(ss));
}

}

namespace std
{

template<>
struct hash<cxxmetrics::metric_value>
{
    std::size_t operator()(const cxxmetrics::metric_value& v) const
    {
        return v.value_.hash_value();
    }
};

}

#endif //CXXMETRICS_METRIC_VALUE_HPP
