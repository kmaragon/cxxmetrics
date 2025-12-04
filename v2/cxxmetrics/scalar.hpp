#pragma once

#include "metric.hpp"

namespace cxxmetrics
{

/**
 * \brief The base type for a scalar value
 */
class scalar : public metric
{
public:
    virtual long double value() const noexcept = 0;

    ~scalar() override = default;
};

} // namespace cxxmetrics
