#ifndef CXXMETRICS_COUNTER_HPP
#define CXXMETRICS_COUNTER_HPP

#include "metric.hpp"
#include <atomic>

namespace cxxmetrics
{

/**
 * \brief A counter that counts values
 */
class counter : public metric<counter>
{
    std::atomic_int_fast64_t value_;
public:
    /**
     * \brief Construct a counter
     *
     * \param initial_value the initial value of the counter
     */
    explicit counter(int64_t initial_value = 0) noexcept;

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

    virtual ~counter() = default;

    counter &operator=(const counter &c) noexcept;
    counter &operator=(counter &&c) noexcept;

    /**
     * \brief explicitly set the counter to a value
     *
     * \param value the value to set the counter to
     * \return a reference to the counter
     */
    counter &operator=(int64_t value) noexcept;

    /**
     * \brief increment the counter by the specified value
     *
     * \param by the amount by which to increment the counter
     *
     * \return the value of the counter after the increment
     */
    virtual int64_t incr(int64_t by) noexcept;

    /**
     * \brief Get the current value of the counter
     *
     * \return the current value of the counter
     */
    int64_t value() const noexcept;

    /**
     * \brief Convenience cast operator
     */
    inline operator int64_t() const
    {
        return value();
    }

    /**
     * \brief Convenience operator to increment the counter by 1
     *
     * \return a reference to the counter
     */
    inline counter &operator++()
    {
        incr(1);
        return *this;
    }

    /**
     * \brief Convenience operator to increment the counter by a value
     *
     * \param by the amount by which to increment the counter
     *
     * \return a reference to the counter
     */
    inline counter &operator+=(int64_t by)
    {
        incr(by);
        return *this;
    }

    /**
     * \brief Convenience operator to decrement the counter by 1
     *
     * \return a reference to the counter
     */
    inline counter &operator--()
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
    inline counter &operator-=(int64_t by)
    {
        incr(-by);
        return *this;
    }
};

}

#endif //CXXMETRICS_COUNTER_HPP
