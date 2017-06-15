#include "counter.hpp"

using namespace cxxmetrics;

counter::counter(int64_t initial_value) noexcept:
    value_(initial_value)
{}


counter::counter(const counter &c) noexcept :
        metric(c),
        value_(c.value_.load(std::memory_order_relaxed))
{}

counter::counter(counter &&c) noexcept :
    metric(c),
    value_(c.value_.load(std::memory_order_relaxed))
{
    c.value_ = 0;
}

counter& counter::operator=(const counter &c) noexcept
{
    value_ = c;
    return *this;
}

counter& counter::operator=(counter &&c) noexcept
{
    value_ = c;
    c.value_ = 0;
    return *this;
}

counter& counter::operator=(int64_t value) noexcept
{
    value_ = value;
}

int64_t counter::incr(int64_t by) noexcept
{
    return value_ += by;
}

int64_t counter::value() const noexcept
{
    return value_;
}