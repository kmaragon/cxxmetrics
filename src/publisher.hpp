#ifndef CXXMETRICS_PUBLISHER_HPP
#define CXXMETRICS_PUBLISHER_HPP

#include "metrics_registry.hpp"

namespace cxxmetrics
{

/**
 * \brief The base class for the metrics publisher
 *
 * The publisher simply uses the standard visit pattern to access metrics and publish.
 * However, inheriting from this class allows the publisher to gain additional insight into
 * metrics that are registered in the underlying registry.
 *
 * \tparam TMetricRepo the repository being used by the registry
 */
template<typename TMetricRepo>
class metric_publisher
{
    metrics_registry<TMetricRepo>& registry_;

protected:
    /**
     * \brief Get a piece of data from the registry. Generally, this will be a type specific to the publisher
     *
     * This allows the publisher to store data in the registry that is specific to the publisher. If the data is
     * already registered, the returned value will be the registered data. Otherwise, the object is constructed
     * and stored in the registry using the arguments specified.
     *
     * \tparam TDataType the type of data to get from the registry
     * \tparam TBuildArgs the types of arguments that will be used to construct the data if it doesn't exist
     *
     * \return the registered data of the requested type
     */
    template<typename TDataType, typename... TBuildArgs>
    TDataType& get_data(TBuildArgs&&... args);

    /**
     * \brief Get a piece of data from the registry for a specific metric.
     *
     * This returns a pointer which has the potential for being null. This can happen when the metric_path being
     * requested isn't registered in the registry (thus there's nothing to attach data to).
     *
     * \tparam TDataType the type of data to attach to the metric
     * \tparam TBuildArgs The types of arguments that will be used to construct the data if it doesn't exist but the path does
     *
     * \param path the path of the metric to attach the data to
     *
     * \return the attached data if the path existed
     */
    template<typename TDataType, typename... TBuildArgs>
    TDataType* get_data_for(const metric_path& path);

    /**
     * \brief Get whether or not there is a metric at the specified path
     *
     * \return whether or not there is a metric at the specified path
     */
    bool has_metric(const metric_path& path) const;

    /**
     * \brief Get a string representation of the type of metric stored at the specified path
     *
     * If there is no metric stored at the path, this will return an empty string
     *
     * \param path the path of the metric to query for the type
     *
     * \return the type of metric at the specified path
     */
    std::string metric_type(const metric_path& path) const;

    /**
     * \brief Visit just a single metric in the registry
     *
     * The handler follows the same signature as the visitor on the registry at large
     *
     * \note this method may hold a lock on the registry data at some level. It is therefore advised not to use any other registry access routines inside of the handler
     *
     * \tparam THandler the handler to call if the registry is found at the specified path, the first argument will be the path
     * \param path the path of the metric to visit
     */
    template<typename THandler>
    void visit_one(const metric_path& path, THandler&& handler) const;

    /**
     * \brief a convenience wrapper around \refitem metrics_registry::visit_registered_metrics
     */
    template<typename THandler>
    void visit_all(THandler&& handler);
public:
    /**
     * \brief Construct a publisher that will publish from the specified registry
     *
     * \param registry the registry from which the publisher will publish
     */
    metric_publisher(metrics_registry<TMetricRepo>& registry);
};

}

#endif //CXXMETRICS_PUBLISHER_HPP
