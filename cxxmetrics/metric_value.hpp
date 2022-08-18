#ifndef CXXMETRICS_METRIC_VALUE_HPP
#define CXXMETRICS_METRIC_VALUE_HPP

#include <chrono>
#include <string>
#include <cmath>
#include <cstddef>
#include "time.hpp"

namespace cxxmetrics
{

namespace internal
{

class variant_data
{
public:
    virtual ~variant_data() = default;
    virtual std::string to_string() const = 0;
    virtual long long to_integral(bool* valid) const = 0;
    virtual long double to_float(bool* valid) const = 0;
    virtual std::chrono::nanoseconds to_nanos(bool* valid) const = 0;
    virtual int type_score() const = 0;
    virtual std::size_t hash_value() const = 0;
    virtual void copy(void* into) const noexcept = 0;
    virtual void move(void* into) noexcept { return copy(into); }
    virtual void add(const variant_data& other) noexcept = 0;
    virtual void multiply(const variant_data& other) noexcept = 0;
    virtual void divide(const variant_data& other) noexcept = 0;
    virtual void negate() noexcept = 0;
    virtual void bitwise_negate() noexcept = 0;
    virtual int compare(const variant_data& other) const noexcept = 0;
};

template<typename T>
class integral_variant_data : public variant_data
{
    static_assert(std::is_integral<T>::value, "Expected an integral type for integral variant data");
    T val_;
public:
    integral_variant_data(T value) :
            val_(value)
    { }

    std::string to_string() const override
    {
        return std::to_string(val_);
    }

    long long to_integral(bool* valid) const override
    {
        if (valid)
            *valid = true;
        return static_cast<long long>(val_);
    }

    long double to_float(bool* valid) const override
    {
        if (valid)
            *valid = true;
        return static_cast<long double>(val_);
    }

    std::chrono::nanoseconds to_nanos(bool* valid) const override
    {
        return std::chrono::nanoseconds(to_integral(valid));
    }

    std::size_t hash_value() const override
    {
        return std::hash<T>()(val_);
    }

    void copy(void* into) const noexcept override
    {
        new (reinterpret_cast<integral_variant_data<T>*>(into)) integral_variant_data<T>(val_);
    }

    int type_score() const override
    {
        return (sizeof(T) * 10) + (std::is_signed<T>::value ? 0 : 1);
    }

    void add(const variant_data& other) noexcept override
    {
        bool valid;
        auto lv = other.to_integral(&valid);
        if (valid)
            val_ += lv;
        else
            val_ += other.to_float(nullptr);
    }

    void multiply(const variant_data& other) noexcept override
    {
        bool valid;
        auto lv = other.to_integral(&valid);
        if (valid)
        {
            val_ *= lv;
        }
        else
        {
            auto fv = other.to_float(&valid);
            if (valid)
                val_ *= fv;
        }
    }

    void divide(const variant_data& other) noexcept override
    {
        bool valid;
        auto lv = other.to_integral(&valid);
        if (valid)
        {
            if (!lv)
                val_ = 0;
            else
                val_ /= lv;
        }
        else
        {
            auto fv = other.to_float(&valid);
            if (valid)
            {
                if (!fv)
                    val_ = 0;
                else
                    val_ /= fv;
            }
        }
    }

    void negate() noexcept override
    {
        val_ = -val_;
    }

    void bitwise_negate() noexcept override
    {
        val_ = ~val_;
    }

    int compare(const variant_data& other) const noexcept override
    {
        bool valid;
        auto lv = static_cast<T>(other.to_integral(&valid));
        if (!valid)
        {
            auto fv = other.to_float(&valid);
            if (!valid)
                return -1;

            return val_ < fv ? -1 : val_ == fv ? 0 : 1;
        }

        return val_ - lv;
    }
};

template<typename T>
class float_variant_data : public variant_data
{
    static_assert(std::is_floating_point<T>::value, "Expected an integral type for integral variant data");
    T val_;
public:
    float_variant_data(T value) :
            val_(value)
    { }

    virtual std::string to_string() const override
    {
        return std::to_string(val_);
    }

    virtual long long to_integral(bool* valid) const override
    {
        if (valid)
            *valid = true;
        return static_cast<long long>(std::round(val_));
    }

    virtual long double to_float(bool* valid) const override
    {
        if (valid)
            *valid = true;
        return static_cast<long double>(val_);
    }

    std::chrono::nanoseconds to_nanos(bool* valid) const override
    {
        if (valid)
            *valid = true;
        return std::chrono::nanoseconds(to_integral(nullptr));
    }

    std::size_t hash_value() const override
    {
        return std::hash<T>()(val_);
    }

    void copy(void* into) const noexcept override
    {
        new (reinterpret_cast<float_variant_data<T>*>(into)) float_variant_data<T>(val_);
    }

    int type_score() const override
    {
        return (sizeof(T) * 20);
    }

    void add(const variant_data& other) noexcept override
    {
        val_ += other.to_float(nullptr);
    }

    void multiply(const variant_data& other) noexcept override
    {
        bool valid;
        auto fv = other.to_float(&valid);
        if (valid)
            val_ *= fv;
    }

    void divide(const variant_data& other) noexcept override
    {
        bool valid;
        auto fv = other.to_float(&valid);
        if (valid)
        {
            if (!fv)
                val_ = 0;
            else
                val_ /= fv;
        }
    }

    void negate() noexcept override
    {
        val_ = -val_;
    }

    void bitwise_negate() noexcept override
    {
    }

    int compare(const variant_data& other) const noexcept override
    {
        bool valid;
        auto fv = static_cast<T>(other.to_float(&valid));
        if (!valid)
            return -1;

        return val_ < fv ? -1 : val_ == fv ? 0 : 1;
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

    long long to_integral(bool* valid) const override
    {
        static bool ign;
        if (valid == nullptr)
            valid = &ign;
        if (!val_.length())
        {
            *valid = false;
            return 0;
        }

        char* end = nullptr;
        if (val_[0] == '-')
        {
            auto res = strtoll(val_.c_str(), &end, 10);
            if (!end || *end)
            {
                *valid = false;
                return 0;
            }

            *valid = true;
            return res;
        }

        auto res = strtoull(val_.c_str(), &end, 10);
        if (!end || *end)
        {
            *valid = false;
            return 0;
        }
        *valid = true;
        return res;
    }

    long double to_float(bool *valid) const override
    {
        static bool ign;
        if (valid == nullptr)
            valid = &ign;
        if (!val_.length())
        {
            *valid = false;
            return std::numeric_limits<long double>::quiet_NaN();
        }

        char* end = nullptr;
        auto res = strtold(val_.c_str(), &end);
        if (!end || *end)
        {
            *valid = false;
            return std::numeric_limits<long double>::quiet_NaN();
        }
        *valid = true;
        return res;
    }

    std::chrono::nanoseconds to_nanos(bool* valid) const override
    {
        bool lvalid;
        auto lv = to_integral(&lvalid);
        if (lvalid)
            return std::chrono::nanoseconds(lv);

        return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::duration<long double, std::chrono::nanoseconds::period>(to_float(nullptr)));
    }

    std::size_t hash_value() const override
    {
        return std::hash<std::string>()(val_);
    }

    void copy(void* into) const noexcept override
    {
        new (reinterpret_cast<string_variant_data*>(into)) string_variant_data(val_);
    }

    void move(void* into) noexcept override
    {
        new (reinterpret_cast<string_variant_data*>(into)) string_variant_data(std::move(val_));
    }

    int type_score() const override
    {
        return 1;
    }

    void add(const variant_data& other) noexcept override
    {
        val_ += other.to_string();
    }

    void multiply(const variant_data& other) noexcept override
    {
    }

    void divide(const variant_data& other) noexcept override
    {
    }

    void negate() noexcept override
    {
        bool valid;
        auto lv = to_integral(&valid);
        if (valid)
        {
            val_ = std::to_string(-lv);
            return;
        }

        auto fv = to_float(&valid);
        if (valid)
            val_ = std::to_string(-fv);
    }

    void bitwise_negate() noexcept override
    {
        bool valid;
        auto lv = to_integral(&valid);
        if (valid)
        {
            val_ = std::to_string(~lv);
            return;
        }
    }

    int compare(const variant_data& other) const noexcept override
    {
        return val_.compare(other.to_string());
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

    long long to_integral(bool* valid) const override
    {
        if (valid)
            *valid = true;
        return static_cast<long long>(dur_.count());
    }

    long double to_float(bool* valid) const override
    {
        if (valid)
            *valid = true;
        return static_cast<long double>(dur_.count());
    }

    std::chrono::nanoseconds to_nanos(bool* valid) const override
    {
        if (valid)
            *valid = true;
        return std::chrono::duration_cast<std::chrono::nanoseconds>(dur_);
    }

    std::size_t hash_value() const override
    {
        return std::hash<std::chrono::duration<TRep, TPeriod>>()(dur_);
    }

    void copy(void* into) const noexcept override
    {
        new (reinterpret_cast<duration_variant_data<TRep, TPeriod>*>(into)) duration_variant_data<TRep, TPeriod>(dur_);
    }

    int type_score() const override
    {
        if (std::is_floating_point<TRep>::value)
            return (sizeof(TRep) * 20) + 2;
        else
            return (sizeof(TRep) * 10) + 2;
    }

    void add(const variant_data& other) noexcept override
    {
        dur_ += std::chrono::duration_cast<std::chrono::duration<TRep, TPeriod>>(other.to_nanos(nullptr));
    }

    void multiply(const variant_data& other) noexcept override
    {
        bool valid;
        auto lv = other.to_integral(&valid);
        if (!valid)
        {
            auto fv = other.to_float(&valid);
            if (!valid)
                return;

            dur_ *= fv;
            return;
        }

        dur_ *= lv;
    }

    void divide(const variant_data& other) noexcept override
    {
        bool valid;
        auto lv = other.to_integral(&valid);
        if (!valid)
        {
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

    void negate() noexcept override
    {
        dur_ = -dur_;
    }

    void bitwise_negate() noexcept override
    {
    }

    int compare(const variant_data& other) const noexcept override
    {
        bool valid;
        auto n = other.to_nanos(&valid);
        return dur_ < n ? -1 : dur_ == n ? 0 : 1;
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

    variant_data_holder(const variant_data* cp)
    {
        cp->copy(std::addressof(v));
    }

    variant_data_holder uc_add(const variant_data_holder& other) const
    {
        variant_data_holder result(as<variant_data>());
        result.as<variant_data>()->add(*other.as<variant_data>());

        return result;
    }

    variant_data_holder uc_mult(const variant_data_holder& other) const
    {
        variant_data_holder result(as<variant_data>());
        result.as<variant_data>()->multiply(*other.as<variant_data>());

        return result;
    }

    variant_data_holder uc_div(const variant_data_holder& other) const
    {
        variant_data_holder result(as<variant_data>());
        result.as<variant_data>()->divide(*other.as<variant_data>());

        return result;
    }

    template<typename T, typename>
    struct float_int_initializer;

    template<typename _T>
    struct float_int_initializer<_T, typename std::enable_if<std::is_integral<_T>::value, void>::type>
    {
        void operator()(void* data, _T value) const
        {
            new (data) integral_variant_data<_T>(value);
        }
    };

    template<typename _T>
    struct float_int_initializer<_T, typename std::enable_if<std::is_floating_point<_T>::value, void>::type>
    {
        void operator()(void* data, _T value) const
        {
            new (data) float_variant_data<_T>(value);
        }
    };

public:
    template<typename TInt, typename = typename std::enable_if<std::is_integral<TInt>::value || std::is_floating_point<TInt>::value, void>::type>
    variant_data_holder(TInt value)
    {
        float_int_initializer<TInt, void> init;
        init(as<void>(), value);
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

    variant_data_holder negate() const
    {
        variant_data_holder result(as<variant_data>());
        result.as<variant_data>()->negate();

        return result;
    }

    variant_data_holder bitwise_negate() const
    {
        variant_data_holder result(as<variant_data>());
        result.as<variant_data>()->bitwise_negate();

        return result;
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

    long long to_integral() const
    {
        return as<variant_data>()->to_integral(nullptr);
    }

    std::string to_string() const
    {
        return as<variant_data>()->to_string();
    }

    long double to_float() const
    {
        return as<variant_data>()->to_float(nullptr);
    }

    std::chrono::nanoseconds to_nanos() const
    {
        return as<variant_data>()->to_nanos(nullptr);
    }

    int compare(const variant_data_holder& other) const
    {
        return as<variant_data>()->compare(*other.as<variant_data>());
    }

    variant_data_holder add(const variant_data_holder& other) const
    {
        if (other.as<variant_data>()->type_score() > as<variant_data>()->type_score())
            return other.uc_add(*this);
        return uc_add(other);
    }

    variant_data_holder multiply(const variant_data_holder& other) const
    {
        if (other.as<variant_data>()->type_score() > as<variant_data>()->type_score())
            return other.uc_mult(*this);
        return uc_mult(other);
    }

    variant_data_holder divide(const variant_data_holder& other) const
    {
        if (other.as<variant_data>()->type_score() > as<variant_data>()->type_score())
            return other.uc_div(*this);
        return uc_div(other);
    }

    auto hash_value() const
    {
        return as<variant_data>()->hash_value();
    }

};

}

class metric_value
{
    internal::variant_data_holder value_;

    metric_value(internal::variant_data_holder&& nv) :
            value_(std::move(nv))
    { }

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
            value_(std::move(other.value_))
    { }

    metric_value& operator=(metric_value&& other) noexcept
    {
        value_ = std::move(other.value_);
        return *this;
    }

    metric_value& operator+=(const metric_value& other)
    {
        value_ = (value_.add(other.value_));
        return *this;
    }

    metric_value& operator-=(const metric_value& other)
    {
        value_ = (value_.add(other.value_.negate()));
        return *this;
    }

    metric_value& operator/=(const metric_value& other)
    {
        value_ = (value_.divide(other.value_));
        return *this;
    }

    metric_value& operator*=(const metric_value& other)
    {
        value_ = (value_.multiply(other.value_));
        return *this;
    }

    metric_value operator+(const metric_value& other) const
    {
        return metric_value(value_.add(other.value_));
    }

    metric_value operator-(const metric_value& other) const
    {
        return metric_value(value_.add(other.value_.negate()));
    }

    metric_value operator/(const metric_value& other) const
    {
        return metric_value(value_.divide(other.value_));
    }

    metric_value operator*(const metric_value& other) const
    {
        return metric_value(value_.multiply(other.value_));
    }

    metric_value operator-() const
    {
        return metric_value(value_.negate());
    }

    metric_value operator~() const
    {
        return metric_value(value_.bitwise_negate());
    }

    bool operator>(const metric_value& other) const
    {
        return value_.compare(other.value_) > 0;
    }

    bool operator>=(const metric_value& other) const
    {
        return value_.compare(other.value_) >= 0;
    }

    bool operator<(const metric_value& other) const
    {
        return value_.compare(other.value_) < 0;
    }

    bool operator<=(const metric_value& other) const
    {
        return value_.compare(other.value_) <= 0;
    }

    bool operator==(const metric_value& other) const
    {
        return value_.compare(other.value_) == 0;
    }

    bool operator!=(const metric_value& other) const
    {
        return value_.compare(other.value_) != 0;
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

    template<typename TRep, typename TPer>
    operator std::chrono::duration<TRep, TPer>() const
    {
        return std::chrono::duration_cast<std::chrono::duration<TRep, TPer>>(value_.to_nanos());
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
    std::size_t operator()(const cxxmetrics::metric_value &v) const
    {
        return v.value_.hash_value();
    }
};

namespace chrono
{

template<typename _ToDur>
constexpr _ToDur duration_cast(const cxxmetrics::metric_value &v)
{
    return duration_cast<_ToDur>(static_cast<std::chrono::nanoseconds>(v));
}

}

}
#endif //CXXMETRICS_METRIC_VALUE_HPP
