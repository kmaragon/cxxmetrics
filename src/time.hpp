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

/**
 * \brief a constexpr period of time used in metric templates for things like time windows
 */
class period
{
public:
    using value = templates::duration_type;

private:
    const value value_;
public:

    constexpr period(value v) :
            value_(v) {}

    constexpr operator templates::duration_type() const
    {
        return (templates::duration_type) value_;
    }

    constexpr std::chrono::steady_clock::duration to_duration() const
    {
        return std::chrono::microseconds(value_);
    }

    constexpr operator std::chrono::steady_clock::duration() const
    {
        return to_duration();
    }

};

namespace time
{

constexpr period microseconds(period::value v)
{
    return v;
}

constexpr period milliseconds(period::value v)
{
    return microseconds(v * 1000);
}

constexpr period seconds(period::value v)
{
    return milliseconds(v * 1000);
}

constexpr period minutes(period::value v)
{
    return seconds(v * 60);
}

constexpr period hours(period::value v)
{
    return minutes(v * 60);
}

}

}

namespace cxxmetrics_literals
{

constexpr cxxmetrics::period operator""_micro(cxxmetrics::period::value v)
{
    return cxxmetrics::time::microseconds(v);
}

constexpr cxxmetrics::period operator""_msec(cxxmetrics::period::value v)
{
    return cxxmetrics::time::milliseconds(v);
}

constexpr cxxmetrics::period operator""_sec(cxxmetrics::period::value v)
{
    return cxxmetrics::time::seconds(v);
}

constexpr cxxmetrics::period operator""_min(cxxmetrics::period::value v)
{
    return cxxmetrics::time::minutes(v);
}

constexpr cxxmetrics::period operator""_hour(cxxmetrics::period::value v)
{
    return cxxmetrics::time::hours(v);
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
