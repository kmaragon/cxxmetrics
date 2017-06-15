#ifndef CXXMETRICS_METER_HPP
#define CXXMETRICS_METER_HPP

#include "ewma.hpp"

namespace cxxmetrics
{

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

template<>
class meter<duration::minutes<1>, duration::minutes<5>, duration::minutes<15>> : public _meter_impl<duration::minutes<1>, duration::minutes<5>, duration::minutes<15>>
{};

}

#endif //CXXMETRICS_METER_HPP
