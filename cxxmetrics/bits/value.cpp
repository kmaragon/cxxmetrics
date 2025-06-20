#include "value.hpp"
#include <cmath>
#include <limits>
#include <chrono>
#include <string>

namespace cxxmetrics {
namespace detail {

// string_variant_data implementation
string_variant_data::string_variant_data(std::string value) : val_(std::move(value)) {}

void string_variant_data::serialize(fmt::memory_buffer &onto) const {
    onto.append(val_);
}

long long string_variant_data::to_integral(bool *valid) const {
    static thread_local bool ign;
    if (valid == nullptr)
        valid = &ign;
    if (!val_.length()) {
        *valid = false;
        return 0;
    }

    long long res;
    auto rc = std::from_chars(val_.data(), val_.data() + val_.size(), res);
    if (rc.ec != std::errc{}) {
        *valid = false;
        return 0;
    }

    if (rc.ptr != val_.data() + val_.size()) {
        *valid = false;
    } else {
        *valid = true;
    }

    return res;
}

long double string_variant_data::to_float(bool *valid) const {
    static thread_local bool ign;
    if (valid == nullptr)
        valid = &ign;
    if (!val_.length()) {
        *valid = false;
        return std::numeric_limits<long double>::quiet_NaN();
    }

    long double res;
    auto rc = std::from_chars(val_.data(), val_.data() + val_.size(), res);
    if (rc.ec != std::errc{}) {
        *valid = false;
        return std::numeric_limits<long double>::quiet_NaN();
    }

    if (rc.ptr != val_.data() + val_.size()) {
        *valid = false;
    } else {
        *valid = true;
    }

    return res;
}

std::chrono::nanoseconds string_variant_data::to_nanos(bool *valid) const {
    bool lvalid;
    auto lv = to_float(&lvalid);
    if (lvalid) {
        if (valid != nullptr)
            *valid = true;
        return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(1) * lv);
    }

    return std::chrono::seconds(to_integral(valid));
}

std::size_t string_variant_data::hash_value() const noexcept {
    return std::hash<std::string>{}(val_);
}

void string_variant_data::copy(void *into) const noexcept {
    new (reinterpret_cast<string_variant_data *>(into))
        string_variant_data(val_);
}

void string_variant_data::move(void *into) noexcept {
    new (reinterpret_cast<string_variant_data *>(into))
        string_variant_data(std::move(val_));
}

int string_variant_data::type_score() const { return 1; }

void string_variant_data::add(const variant_data &other) noexcept {
    bool valid;
    auto self_i = to_integral(&valid);
    if (valid) {
        auto other_i = other.to_integral(&valid);
        if (valid) {
            val_ = std::to_string(self_i + other_i);
            return;
        }

        auto other_f = other.to_float(&valid);
        if (valid) {
            val_ = std::to_string(self_i + other_f);
            return;
        }
    }

    auto self_f = to_float(&valid);
    if (valid) {
        auto other_i = other.to_integral(&valid);
        if (valid) {
            val_ = std::to_string(self_f + other_i);
            return;
        }

        auto other_f = other.to_float(&valid);
        if (valid) {
            val_ = std::to_string(self_f + other_f);
            return;
        }
    }

    static thread_local fmt::memory_buffer buf;
    buf.clear();
    other.serialize(buf);
    val_.append(buf.data(), buf.size());
}

void string_variant_data::multiply(const variant_data &other) noexcept {
    bool valid;
    auto lv = to_integral(&valid);
    if (valid) {
        val_ = std::to_string(lv * other.to_integral(nullptr));
        return;
    }

    auto fv = to_float(&valid);
    if (valid)
        val_ = std::to_string(fv * other.to_float(nullptr));
}

void string_variant_data::divide(const variant_data &other) noexcept {
    bool valid;
    auto lv = to_integral(&valid);
    if (valid) {
        val_ = std::to_string(lv * other.to_integral(nullptr));
        return;
    }

    auto fv = to_float(&valid);
    if (valid)
        val_ = std::to_string(fv * other.to_float(nullptr));
}

void string_variant_data::negate() noexcept {
    bool valid;
    auto lv = to_integral(&valid);
    if (valid) {
        val_ = std::to_string(-lv);
        return;
    }

    auto fv = to_float(&valid);
    if (valid)
        val_ = std::to_string(-fv);
}

void string_variant_data::bitwise_negate() noexcept {
    bool valid;
    auto lv = to_integral(&valid);
    if (valid) {
        val_ = std::to_string(~lv);
        return;
    }
}

int string_variant_data::compare(const variant_data &other) const noexcept {
    static thread_local fmt::memory_buffer buf;
    buf.clear();
    other.serialize(buf);

    return val_.compare(std::string_view(buf.data(), buf.size()));
}

// string_view_variant_data implementation
string_view_variant_data::string_view_variant_data(const std::string_view& value) : val_(value) {}

void string_view_variant_data::serialize(fmt::memory_buffer &onto) const {
    onto.append(val_);
}

long long string_view_variant_data::to_integral(bool *valid) const {
    static thread_local bool ign;
    if (valid == nullptr)
        valid = &ign;
    if (!val_.length()) {
        *valid = false;
        return 0;
    }

    long long res;
    auto rc = std::from_chars(val_.data(), val_.data() + val_.size(), res);
    if (rc.ec != std::errc{}) {
        *valid = false;
        return 0;
    }

    if (rc.ptr != val_.data() + val_.size()) {
        *valid = false;
    } else {
        *valid = true;
    }

    return res;
}

long double string_view_variant_data::to_float(bool *valid) const {
    static thread_local bool ign;
    if (valid == nullptr)
        valid = &ign;
    if (!val_.length()) {
        *valid = false;
        return std::numeric_limits<long double>::quiet_NaN();
    }

    long double res;
    auto rc = std::from_chars(val_.data(), val_.data() + val_.size(), res);
    if (rc.ec != std::errc{}) {
        *valid = false;
        return std::numeric_limits<long double>::quiet_NaN();
    }

    if (rc.ptr != val_.data() + val_.size()) {
        *valid = false;
    } else {
        *valid = true;
    }

    return res;
}

std::chrono::nanoseconds string_view_variant_data::to_nanos(bool *valid) const {
    bool lvalid;
    auto lv = to_float(&lvalid);
    if (lvalid) {
        if (valid != nullptr)
            *valid = true;
        return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(1) * lv);
    }

    return std::chrono::seconds(to_integral(valid));
}

std::size_t string_view_variant_data::hash_value() const noexcept {
    return std::hash<std::string_view>{}(val_);
}

void string_view_variant_data::copy(void *into) const noexcept {
    // special case - copy is used when making a copy for mutation
    // like add / subtract / negate etc. Which means we want something
    // mutable
    new (reinterpret_cast<string_variant_data *>(into))
        string_variant_data(std::string{val_});
}

void string_view_variant_data::move(void *into) noexcept {
    new (reinterpret_cast<string_view_variant_data *>(into))
        string_view_variant_data(val_);
}

int string_view_variant_data::type_score() const { return 2; }

void string_view_variant_data::add(const variant_data &other) noexcept {
    // string_view is immutable, so we can't modify it
}

void string_view_variant_data::multiply(const variant_data &other) noexcept {
    // string_view is immutable, so we can't modify it
}

void string_view_variant_data::divide(const variant_data &other) noexcept {
    // string_view is immutable, so we can't modify it
}

void string_view_variant_data::negate() noexcept {
    // string_view is immutable, so we can't modify it
}

void string_view_variant_data::bitwise_negate() noexcept {
    // string_view is immutable, so we can't modify it
}

int string_view_variant_data::compare(const variant_data &other) const noexcept {
    static thread_local fmt::memory_buffer buf;
    buf.clear();
    other.serialize(buf);
    std::string_view other_view(buf.data(), buf.size());
    return val_.compare(other_view);
}

// variant_data_holder implementation
variant_data_holder::variant_data_holder(const variant_data *cp) { cp->copy(std::addressof(v)); }

variant_data_holder variant_data_holder::uc_add(const variant_data_holder &other) const {
    variant_data_holder result(as<variant_data>());
    result.as<variant_data>()->add(*other.as<variant_data>());
    return result;
}

variant_data_holder variant_data_holder::uc_mult(const variant_data_holder &other) const {
    variant_data_holder result(as<variant_data>());
    result.as<variant_data>()->multiply(*other.as<variant_data>());
    return result;
}

variant_data_holder variant_data_holder::uc_div(const variant_data_holder &other) const {
    variant_data_holder result(as<variant_data>());
    result.as<variant_data>()->divide(*other.as<variant_data>());
    return result;
}

variant_data_holder::variant_data_holder(std::string value) {
    new (as<string_variant_data>()) string_variant_data(std::move(value));
}

variant_data_holder::variant_data_holder(const char *value) {
    new (as<string_variant_data>()) string_variant_data(value);
}

variant_data_holder::variant_data_holder(const variant_data_holder &from) {
    from.as<variant_data>()->copy(std::addressof(v));
}

variant_data_holder::variant_data_holder(variant_data_holder &&from) {
    from.as<variant_data>()->move(std::addressof(v));
}

variant_data_holder variant_data_holder::negate() const {
    variant_data_holder result(as<variant_data>());
    result.as<variant_data>()->negate();
    return result;
}

variant_data_holder variant_data_holder::bitwise_negate() const {
    variant_data_holder result(as<variant_data>());
    result.as<variant_data>()->bitwise_negate();
    return result;
}

variant_data_holder::~variant_data_holder() { as<variant_data>()->~variant_data(); }

variant_data_holder &variant_data_holder::operator=(variant_data_holder &&other) noexcept {
    as<variant_data>()->~variant_data();
    other.as<variant_data>()->move(std::addressof(v));
    return *this;
}

long long variant_data_holder::to_integral() const {
    return as<variant_data>()->to_integral(nullptr);
}

void variant_data_holder::serialize(fmt::memory_buffer& onto) const { 
    return as<variant_data>()->serialize(onto); 
}

long double variant_data_holder::to_float() const { 
    return as<variant_data>()->to_float(nullptr); 
}

std::chrono::nanoseconds variant_data_holder::to_nanos() const {
    return as<variant_data>()->to_nanos(nullptr);
}

int variant_data_holder::compare(const variant_data_holder &other) const {
    return as<variant_data>()->compare(*other.as<variant_data>());
}

variant_data_holder variant_data_holder::add(const variant_data_holder &other) const {
    if (other.as<variant_data>()->type_score() >
        as<variant_data>()->type_score())
        return other.uc_add(*this);
    return uc_add(other);
}

variant_data_holder variant_data_holder::multiply(const variant_data_holder &other) const {
    if (other.as<variant_data>()->type_score() >
        as<variant_data>()->type_score())
        return other.uc_mult(*this);
    return uc_mult(other);
}

variant_data_holder variant_data_holder::divide(const variant_data_holder &other) const {
    if (other.as<variant_data>()->type_score() >
        as<variant_data>()->type_score())
        return other.uc_div(*this);
    return uc_div(other);
}

std::size_t variant_data_holder::hash_value() const noexcept {
    return as<variant_data>()->hash_value(); 
}

} // namespace detail

// value class implementation
value::value(const value &other) = default;

value::value(value &&other) : value_(std::move(other.value_)) {}

value &value::operator=(value &&other) noexcept {
    value_ = std::move(other.value_);
    return *this;
}

value &value::operator+=(const value &other) {
    value_ = (value_.add(other.value_));
    return *this;
}

value &value::operator-=(const value &other) {
    value_ = (value_.add(other.value_.negate()));
    return *this;
}

value &value::operator/=(const value &other) {
    value_ = (value_.divide(other.value_));
    return *this;
}

value &value::operator*=(const value &other) {
    value_ = (value_.multiply(other.value_));
    return *this;
}

value value::operator+(const value &other) const {
    return value(value_.add(other.value_));
}

value value::operator-(const value &other) const {
    return value(value_.add(other.value_.negate()));
}

value value::operator/(const value &other) const {
    return value(value_.divide(other.value_));
}

value value::operator*(const value &other) const {
    return value(value_.multiply(other.value_));
}

value value::operator-() const { 
    return value(value_.negate()); 
}

value value::operator~() const {
    return value(value_.bitwise_negate());
}

bool value::operator>(const value &other) const {
    return value_.compare(other.value_) > 0;
}

bool value::operator>=(const value &other) const {
    return value_.compare(other.value_) >= 0;
}

bool value::operator<(const value &other) const {
    return value_.compare(other.value_) < 0;
}

bool value::operator<=(const value &other) const {
    return value_.compare(other.value_) <= 0;
}

bool value::operator==(const value &other) const {
    return value_.compare(other.value_) == 0;
}

bool value::operator!=(const value &other) const {
    return value_.compare(other.value_) != 0;
}

value::operator std::string() const { 
    static thread_local fmt::memory_buffer buf;
    buf.clear();
    
    value_.serialize(buf);
    return std::string{buf.data(), buf.size()};
}

value::value(detail::variant_data_holder &&nv) : value_(std::move(nv)) {}

} // namespace cxxmetrics
