#ifndef CXXMETRICS_METRIC_HPP
#define CXXMETRICS_METRIC_HPP

#include "snapshots.hpp"

#if __cplusplus < 201700L
namespace std {
template<typename T>
using invoke_result = result_of<T>;
}
#endif

namespace cxxmetrics
{

namespace internal
{
class metric
{
public:
    virtual std::string metric_type() const noexcept = 0;
    virtual ~metric() = default;
};

template<typename TMetric>
struct default_metric_builder
{
    TMetric operator()() const
    {
        return TMetric();
    }
};

template<typename TMetric>
inline TMetric metric_default_value()
{
    default_metric_builder<TMetric> m;
    return m();
}

}

/**
 * \brief Base class for a metric
 */
template<typename TMetricType>
class metric : public internal::metric
{
protected:
    metric() = default;

public:

    /**
     * \brief Get the compile time type name of the metric type
     *
     * \return the compile time type name of the metric
     */
    static constexpr const char* type_name()
    {
        return typeid(TMetricType).name();
    }

    /**
     * \brief Get the name of the metric type that the metric implements
     *
     * \return the metric type that the metric implements - taken from TMetricType
     */
    std::string metric_type() const noexcept override;
};

template<typename TMetricType>
std::string metric<TMetricType>::metric_type() const noexcept
{
    return typeid(TMetricType).name();
}

}

#endif //CXXMETRICS_METRIC_HPP
