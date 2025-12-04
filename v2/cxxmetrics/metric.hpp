#pragma once

#include <>

namespace cxxmetrics
{

enum class metric_type
{
    gauge,
    counter,
    meter
};

struct scalar_spec
{
    metric_type type;

};

/**
 * \brief Base class for a metric that can be published from a repository
 */
class metric
{
public:
    /**
     * Get the type of metric that the metric holds
     *
     * \return the type of metric
     */
    [[nodiscard]] virtual metric_type type() const noexcept = 0;

    virtual ~metric() = default;
};

} // namespace cxxmetrics
