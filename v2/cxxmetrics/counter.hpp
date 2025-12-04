#pragma once

#include <type_traits>

#include "gauge.hpp"

namespace cxxmetrics
{

namespace detail
{

template <typename T, typename Self, typename std::conditional<std::is_integral_v<T>, std::true_type, std::false_type>>
class basic_counter;

template <typename T, typename Self>
class basic_counter<T, Self, std::true_type> : public gauge<T>
{
public:
    virtual T incr(T by) noexcept = 0;

    Self& operator+=(T by) noexcept
    {
        this->incr(by);
        return *this;
    }

    Self& operator++() noexcept
    {
        this->incr(T{1});
        return *this;
    }
};

template <typename T, typename Self>
class basic_counter<T, Self, std::false_type> : public gauge<T>
{
public:
    virtual T incr(T by) noexcept = 0;

    Self& operator+=(T by) noexcept
    {
        this->incr(by);
        return *this;
    }
};

} // namespace detail

/**
 * \brief A counter that is monotonically increasing
 *
 * \tparam T The underlying type that the counter manages
 */
template <typename T>
class counter : public detail::basic_counter<T, counter<T>>
{
public:
    metric_type type() const override { return metric_type::counter; }
};

} // namespace cxxmetrics
