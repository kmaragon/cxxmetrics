#ifndef CXXMETRICS_TIME_HPP
#define CXXMETRICS_TIME_HPP

#include <unordered_map>
#include <chrono>
#include "meta.hpp"

namespace cxxmetrics
{

class time_window
{
    const unsigned long long value_;
public:
    constexpr time_window(unsigned long long value) :
            value_(value) {}

    constexpr operator templates::duration_type()
    {
        return (templates::duration_type) value_;
    }

};

// fwd
class period;
constexpr period operator""_micro(unsigned long long value);

/**
 * \brief a constexpr period of time used in metric templates for things like time windows
 */
class period
{
public:
    using value = templates::duration_type;

private:
    const value value_;


    friend constexpr period operator""_micro(value v);
public:

    constexpr period(value v) :
            value_(v) {}

    constexpr operator templates::duration_type() const
    {
        return (templates::duration_type) value_;
    }

    constexpr operator std::chrono::steady_clock::duration() const
    {
        return std::chrono::microseconds(value_);
    }
};

namespace literals {

constexpr period operator""_micro(period::value v)
{
    return v;
}

constexpr period operator""_msec(period::value v)
{
    return operator""_micro(v * 1000);
}

constexpr period operator""_sec(period::value v)
{
    return operator""_msec(v * 1000);
}

constexpr period operator""_min(period::value v)
{
    return operator""_sec(v * 60);
}

constexpr period operator""_hour(period::value v)
{
    return operator""_min(v * 60);
}

constexpr period operator""_day(period::value v)
{
    return operator""_hour(v * 24);
}

}

}

namespace std
{

template<typename _rep, typename ratio>
struct hash<std::chrono::duration<_rep, ratio>>
{
    typedef std::chrono::duration<_rep, ratio> argument_type;
    typedef std::size_t result_type;
    result_type operator()(argument_type const& s) const
    {
        return std::hash<_rep>{}(s.count());
    }
};

}

#endif //CXXMETRICS_TIME_HPP
