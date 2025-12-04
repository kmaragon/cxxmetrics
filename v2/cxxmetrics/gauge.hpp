#pragma once

#include "scalar.hpp"

namespace cxxmetrics
{

/**
 * \brief A gauge is a scalar value that is not necessarily monotonically increasing
 */
template <typename T>
class gauge : public scalar
{
public:
    virtual T get() const noexcept = 0;

    metric_type type() const noexcept override { return metric_type::gauge; }

    long double value() const noexcept override { return static_cast<long double>(this->get()); }
};

} // namespace cxxmetrics
