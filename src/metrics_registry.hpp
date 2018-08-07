#ifndef CXXMETRICS_METRICS_REGISTRY_HPP
#define CXXMETRICS_METRICS_REGISTRY_HPP

#include <mutex>
#include <memory>
#include "counter.hpp"
#include "metric_path.hpp"
#include "tag_collection.hpp"

namespace cxxmetrics
{

/**
 * \brief an exception thrown when a registry action is performed with the wrong metric type
 */
class metric_type_mismatch : public std::exception
{
    std::string existing_;
    std::string desired_;
public:
    metric_type_mismatch(std::string existing_type, std::string desired_type) :
            existing_(std::move(existing_type)),
            desired_(std::move(desired_type))
    { }

    const char* what() const noexcept override
    {
        return "The existing registered metric did not match the desired type";
    }

    /**
     * \brief get the type of metric that already existed in the registry
     */
    std::string existing_metric_type() const
    {
        return existing_;
    }

    /**
     * \brief get the type that was desired but was mismatched in the registry
     */
    std::string desired_metric_type() const
    {
        return desired_;
    }
};

/**
 * \brief The root metric that's registered in a repository
 *
 * A metric is registered in the repository by its path. However, the paths only describe the
 * metric metadata and a container of the actual metrics by their tag. publishers access the metrics
 * by their registered_metric. From there, they can publish per tagset metrics or summaries or both.
 *
 * Publishers are also able to store publisher specific data in the registered metric.
 */
class basic_registered_metric
{
    std::string type_;
    std::unordered_map<tag_collection, std::unique_ptr<internal::metric>> metrics_;
    std::mutex lock_;

protected:

    basic_registered_metric(std::string metric_type) :
            type_(std::move(metric_type))
    { }

    virtual std::unique_ptr<internal::metric> create() const = 0;
public:

    /**
     * \brief Visits all of the metrics with their tag values, calling a handler for each
     *
     * The handler should accept 2 arguments: the first is a tag_collection, which will be the
     * tags associated to the metric. The second will be the actual metric snapshot value
     *
     * \tparam THandler The handler type which ought to be auto-deduced from the parameter
     * \param handler the instance of the handler which will be called for each of the metrics
     */
    template<typename THandler>
    void visit(THandler&& handler) const {
        using namespace std::placeholders;

        for (const auto& pair : metrics_)
        {
            auto shandler = std::bind(handler, pair.first, _1);
            pair.second->visit(shandler);
        }
    }

    /**
     * \brief Get the metric with the specified tags, creating it if it doesn't already exist
     *
     * \tparam TMetricType the type of metric to get - must match the type originally registered with
     * \param tags the tags for which to get the metric
     *
     * \return the metric registered with the specified tags
     */
    template<typename TMetricType>
    TMetricType& tagged(const tag_collection& tags);

    /**
     * \brief Get the type of metric registered
     */
    std::string type() const { return type_; }
};

/**
 * \brief the specialized root metric that will be the real types registered in the repository
 *
 * @tparam TMetricType the type of metric registered in the repository
 */
template<typename TMetricType>
class registered_metric : public basic_registered_metric
{
    static TMetricType ag_;

protected:
    std::unique_ptr<internal::metric> create() const {
        return std::make_unique<TMetricType>();
    }
public:
    registered_metric() :
            basic_registered_metric(ag_.metric_type())
    { }

    template<typename THandler>
    void aggregate(THandler&& handler) const {
        auto ss = ag_.snapshot();
        auto visitor = [&ss](const tag_collection& collection, const auto& sm) {
            ss.merge(sm.snapshot());
        };

        handler(ag_.snapshot());
    }
};

template<typename TMetricType>
TMetricType registered_metric<TMetricType>::ag_{};

/**
 * \brief The default metric repository that registers metrics in a standard unordered map with a mutex lock
 */
template<typename TAlloc>
class basic_default_repository
{
    std::unordered_map<metric_path, registered_metric, std::hash<metric_path>, std::equal_to<metric_path>, TAlloc> metrics_;
    std::mutex lock_;
public:
    basic_default_repository() = default;

    registered_metric& get_or_add(const metric_path& name, const std::string& metric_type);
};

using default_repository = basic_default_repository<std::allocator<std::pair<metric_path, registered_metric>>>;

/**
 * \brief The registry where metrics are registered
 *
 * \tparam TRepository the type of registry to store the metric hierarchy in
 */
template<typename TRepository = default_repository>
class metrics_registry
{
    TRepository repo_;

    template<typename TMetric>
    registered_metric& registered(const metric_path& name);

public:
    template<typename... TRepoArgs>
    metrics_registry(TRepoArgs&&... args);

    metrics_registry(const metrics_registry&) = default;
    metrics_registry(metrics_registry&&) = default;
    ~metrics_registry() = default;

    template<typename TCount = int64_t>
    counter<TCount>& counter(const metric_path& name, const tag_collection& tags = tag_collection());
};

template<typename TMetricType>
TMetricType& basic_registered_metric::tagged(const tag_collection& tags)
{
    std::lock_guard<std::mutex> lock(lock_);
    auto fnd = metrics_.find(tags);
    if (fnd == metrics_.end())
    {
        auto nmetric = std::make_unique<TMetricType>();
        auto result = metrics_.emplace(tags, std::move(nmetric));
        return *static_cast<TMetricType*>(result.first->second.get());
    }

    // the static cast here should be safe as we've already verified the type via ctti
    return *static_cast<TMetricType*>(fnd->second.get());
}

template<typename TAlloc>
registered_metric& basic_default_repository<TAlloc>::get_or_add(const metric_path& name, const std::string& type)
{
    std::lock_guard<std::mutex> lock(lock_);
    auto fnd = metrics_.find(name);
    if (fnd == metrics_.end())
    {
        auto result = metrics_.emplace(name, type);
        return result.first->second;
    }

    if (fnd->second.type() != type)
        throw metric_type_mismatch(fnd->second.type(), type);

    return fnd->second;
}

template<typename TRepository>
template<typename TMetric>
registered_metric& metrics_registry<TRepository>::registered(const metric_path& name)
{
    static TMetric nmetricname;
    return repo_.get_or_add(name, nmetricname.metric_type());
}

template<typename TRepository>
template<typename... TRepoArgs>
metrics_registry<TRepository>::metrics_registry(TRepoArgs &&... args) :
        repo_(std::forward<TRepoArgs>(args)...)
{ }

template<typename TRepository>
template<typename TCount>
counter<TCount>& metrics_registry<TRepository>::counter(const metric_path& name, const tag_collection& tags)
{
    return registered<cxxmetrics::counter<TCount>>(name).template tagged<cxxmetrics::counter<TCount>>(tags);
}

}

#endif //CXXMETRICS_METRICS_REGISTRY_HPP
