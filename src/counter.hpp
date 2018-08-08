#ifndef CXXMETRICS_COUNTER_HPP
#define CXXMETRICS_COUNTER_HPP

#include "metric.hpp"
#include <atomic>

namespace cxxmetrics
{

/**
 * \brief A counter that counts values
 */
template<typename TCount = int64_t>
class counter : public metric<counter<TCount>>
{
    std::atomic<TCount> value_;
public:
    /**
     * \brief Construct a counter
     *
     * \param initial_value the initial value of the counter
     */
    explicit counter(TCount initial_value = 0) noexcept;

    /**
     * \brief Copy constructor
     *
     * \param c the counter to copy
     */
    counter(const counter &c) noexcept;

    /**
     * \brief Move constructor
     *
     * \param c the counter to move from
     */
    counter(counter &&c) noexcept;

    ~counter() = default;

    counter &operator=(const counter &c) noexcept;
    counter &operator=(counter &&c) noexcept;

    /**
     * \brief explicitly set the counter to a value
     *
     * \param value the value to set the counter to
     * \return a reference to the counter
     */
    counter &operator=(TCount value) noexcept;

    /**
     * \brief increment the counter by the specified value
     *
     * \param by the amount by which to increment the counter
     *
     * \return the value of the counter after the increment
     */
    TCount incr(TCount by) noexcept;

    /**
     * \brief Get the current value of the counter
     *
     * \return the current value of the counter
     */
    TCount value() const noexcept;

    /**
     * \brief Convenience cast operator
     */
    operator TCount() const
    {
        return value();
    }

    /**
     * \brief Convenience operator to increment the counter by 1
     *
     * \return a reference to the counter
     */
    counter &operator++()
    {
        incr(1);
        return *this;
    }

    /**
     * \brief Convenience operator to increment the counter by 1 in postfix
     *
     * \return a reference to the counter
     */
    TCount operator++(int)
    {
        return incr(1) - 1;
    }

    /**
     * \brief Convenience operator to increment the counter by a value
     *
     * \param by the amount by which to increment the counter
     *
     * \return a reference to the counter
     */
    counter &operator+=(TCount by)
    {
        incr(by);
        return *this;
    }

    /**
     * \brief Convenience operator to decrement the counter by 1
     *
     * \return a reference to the counter
     */
    counter &operator--()
    {
        incr(-1);
        return *this;
    }

    /**
     * \brief Convenience operator to decrement the counter by a value
     *
     * \param by the amount by which to decrement the counter
     *
     * \return a reference to the counter
     */
    counter &operator-=(TCount by)
    {
        incr(-by);
        return *this;
    }

    /**
     * \brief Get a snapshot of the value as it is
     */
    cumulative_value_snapshot snapshot() const {
        return cumulative_value_snapshot(value());
    }
};

template<typename TCount>
counter<TCount>::counter(TCount initial_value) noexcept:
        value_(initial_value)
{}

template<typename TCount>
counter<TCount>::counter(const counter &c) noexcept :
        metric<counter<TCount>>(c),
        value_(c.value_.load())
{}

template<typename TCount>
counter<TCount>::counter(counter &&c) noexcept :
        metric<counter<TCount>>(c),
        value_(c.value_.load())
{
    c.value_ = 0;
}

template<typename TCount>
counter<TCount> &counter<TCount>::operator=(const counter<TCount> &c) noexcept
{
    value_ = c;
    return *this;
}

template<typename TCount>
counter<TCount> &counter<TCount>::operator=(counter<TCount>  &&c) noexcept
{
    value_ = c;
    c.value_ = 0;
    return *this;
}

template<typename TCount>
counter<TCount> &counter<TCount>::operator=(TCount value) noexcept
{
    value_ = value;
    return *this;
}

template<typename TCount>
TCount counter<TCount>::incr(TCount by) noexcept
{
    return value_ += by;
}

template<typename TCount>
TCount counter<TCount>::value() const noexcept
{
    return value_;
}

}

#endif //CXXMETRICS_COUNTER_HPP
