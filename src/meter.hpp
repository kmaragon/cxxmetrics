#ifndef CXXMETRICS_METER_HPP
#define CXXMETRICS_METER_HPP

#include "ewma.hpp"
#include "meta.hpp"

namespace cxxmetrics
{

namespace timeunits
{
template<templates::duration_type T>
struct _milliseconds
{
    static constexpr templates::duration_type value = T;
};

template<templates::duration_type T>
struct _seconds
{
    static constexpr templates::duration_type value = _milliseconds<T>::value * 1000;
};

template<templates::duration_type T>
struct _minutes
{
    static constexpr templates::duration_type value = _seconds<T>::value * 60;
};

template<templates::duration_type T>
struct _hours
{
    static constexpr templates::duration_type value = _minutes<T>::value * 60;
};

template<templates::duration_type T>
struct _days
{
    static constexpr templates::duration_type value = _hours<T>::value * 24;
};

}

struct rate_period
{
    template<unsigned long long T>
    static constexpr templates::duration_type milliseconds = timeunits::_milliseconds<T>::value;

    template<unsigned long long T>
    static constexpr templates::duration_type seconds = timeunits::_seconds<T>::value;

    template<unsigned long long T>
    static constexpr templates::duration_type minutes = timeunits::_minutes<T>::value;

    template<unsigned long long T>
    static constexpr templates::duration_type hours = timeunits::_hours<T>::value;

    template<unsigned long long T>
    static constexpr templates::duration_type days = timeunits::_days<T>::value;
};

template<typename... TDurations>
class _meter_impl
{

};

template<typename... TDurations>
class meter : public _meter_impl<TDurations...>
{
public:
    meter();
};

}

#endif //CXXMETRICS_METER_HPP
