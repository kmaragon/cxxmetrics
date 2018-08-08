#ifndef CXXMETRICS_METRICS_REGISTRY_HPP
#define CXXMETRICS_METRICS_REGISTRY_HPP

#include <mutex>
#include <memory>
#include "counter.hpp"
#include "metric_path.hpp"
#include "tag_collection.hpp"

namespace cxxmetrics
{
template<typename TRepository>
class metrics_registry;

namespace internal
{

class registered_snapshot_visitor_builder
{
public:
    virtual std::size_t visitor_size() const = 0;
    virtual void construct(snapshot_visitor* location, const tag_collection& collection) = 0;
};

template<typename TVisitor>
class invokable_snapshot_visitor_builder : public registered_snapshot_visitor_builder
{
    TVisitor visitor_;
    using visitor_type = decltype(std::bind(std::declval<TVisitor>(), std::declval<tag_collection&>(), std::placeholders::_1));
public:
    invokable_snapshot_visitor_builder(TVisitor&& visitor) :
            visitor_(std::forward<TVisitor>(visitor))
    { }

    std::size_t visitor_size() const override
    {
        return sizeof(invokable_snapshot_visitor<visitor_type>);
    }

    void construct(snapshot_visitor* location, const tag_collection& collection) override
    {
        using namespace std::placeholders;
        new (location) invokable_snapshot_visitor<visitor_type>(std::bind(visitor_, std::forward<const tag_collection&>(collection), _1));
    }
};

}

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

    template<typename TMetricType>
    TMetricType& tagged(const tag_collection& tags)
    {
        return *static_cast<TMetricType*>(this->child(tags));
    }

    template<typename TRepository>
    friend class metrics_registry;
protected:
    virtual void visit_each(internal::registered_snapshot_visitor_builder& builder) = 0;
    virtual void aggregate_all(snapshot_visitor& visitor) = 0;
    virtual internal::metric* child(const tag_collection& tags) = 0;

public:
    basic_registered_metric(const std::string& type) :
            type_(type)
    { }

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
    void visit(THandler&& handler) {
        internal::invokable_snapshot_visitor_builder<THandler> builder(std::forward<THandler>(handler));
        this->visit_each(builder);
    }

    template<typename THandler>
    void aggregate(THandler&& handler) {
        invokable_snapshot_visitor<THandler> visitor(std::forward<THandler>(handler));
        this->aggregate_all(visitor);
    }

    /**
     * \brief Get the type of metric registered
     */
    virtual std::string type() const { return type_; }
};

/**
 * \brief the specialized root metric that will be the real types registered in the repository
 *
 * @tparam TMetricType the type of metric registered in the repository
 */
template<typename TMetricType>
class registered_metric : public basic_registered_metric
{
    std::unordered_map<tag_collection, TMetricType> metrics_;
    std::mutex lock_;

protected:
    void visit_each(internal::registered_snapshot_visitor_builder& builder) override;
    void aggregate_all(snapshot_visitor& visitor) override;
    internal::metric* child(const tag_collection& tags) override;

public:
    registered_metric(const std::string& metric_type_name) :
            basic_registered_metric(metric_type_name)
    { }
};

template<typename TMetricType>
void registered_metric<TMetricType>::visit_each(cxxmetrics::internal::registered_snapshot_visitor_builder &builder)
{
    std::lock_guard<std::mutex> lock(lock_);
    for (auto& p : metrics_)
    {
        auto sz = builder.visitor_size() + sizeof(std::max_align_t);
        void* ptr = alloca(sz);

        std::align(sizeof(std::max_align_t), sz, ptr, sz);
        auto loc = reinterpret_cast<snapshot_visitor*>(ptr);
        builder.construct(loc, p.first);
        try
        {
            loc->visit(p.second.snapshot());
        }
        catch (...)
        {
            loc->~snapshot_visitor();
            throw;
        }

        loc->~snapshot_visitor();
    }
}

template<typename TMetricType>
void registered_metric<TMetricType>::aggregate_all(snapshot_visitor &visitor)
{
    std::unique_lock<std::mutex> lock(lock_);

    auto itr = metrics_.begin();
    if (itr == metrics_.end())
        return;

    auto result = itr->second.snapshot();
    for (++itr; itr != metrics_.end(); ++itr)
    {
        result.merge(itr->second.snapshot());
    }

    lock.unlock();
    visitor.visit(result);
}

template<typename TMetricType>
internal::metric* registered_metric<TMetricType>::child(const cxxmetrics::tag_collection &tags)
{
    std::lock_guard<std::mutex> lock(lock_);
    auto res = metrics_.find(tags);

    if (res != metrics_.end())
        return &res->second;

    return &metrics_.emplace(tags, TMetricType()).first->second;
}

/**
 * \brief The default metric repository that registers metrics in a standard unordered map with a mutex lock
 */
template<typename TAlloc>
class basic_default_repository
{
    std::unordered_map<metric_path, std::unique_ptr<basic_registered_metric>, std::hash<metric_path>, std::equal_to<metric_path>, TAlloc> metrics_;
    std::mutex lock_;
public:
    basic_default_repository() = default;

    template<typename TMetricPtrBuilder>
    basic_registered_metric& get_or_add(const metric_path& name, const TMetricPtrBuilder& builder);

    template<typename THandler>
    void visit(THandler&& handler);

};

template<typename TAlloc>
template<typename TMetricPtrBuilder>
basic_registered_metric& basic_default_repository<TAlloc>::get_or_add(const metric_path& name, const TMetricPtrBuilder& builder)
{
    std::lock_guard<std::mutex> lock(lock_);
    auto existing = metrics_.find(name);

    if (existing == metrics_.end())
    {
        auto ptr = builder();
        return *metrics_.emplace(name, std::move(ptr)).first->second;
    }

    return *existing->second;
}

template<typename TAlloc>
template<typename THandler>
void basic_default_repository<TAlloc>::visit(THandler&& handler)
{
    std::lock_guard<std::mutex> lock(lock_);
    for (auto& pair : metrics_)
        handler(pair.first, *pair.second);
}

using default_repository = basic_default_repository<std::allocator<std::pair<metric_path, basic_registered_metric>>>;

/**
 * \brief The registry where metrics are registered
 *
 * \tparam TRepository the type of registry to store the metric hierarchy in
 */
template<typename TRepository = default_repository>
class metrics_registry
{
    TRepository repo_;

    template<typename TMetricType>
    registered_metric<TMetricType>& get(const metric_path& path);

    template<typename TMetricType>
    TMetricType& get(const metric_path& path, const tag_collection& tags);

public:
    template<typename... TRepoArgs>
    metrics_registry(TRepoArgs&&... args);

    metrics_registry(const metrics_registry&) = default;
    metrics_registry(metrics_registry&& other) noexcept :
            repo_(std::move(other.repo_))
    { }
    ~metrics_registry() = default;

    template<typename THandler>
    void visit_registered_metrics(THandler&& handler);

    template<typename TCount = int64_t>
    counter<TCount>& counter(const metric_path& name, const tag_collection& tags = tag_collection());
};

template<typename TRepository>
template<typename... TRepoArgs>
metrics_registry<TRepository>::metrics_registry(TRepoArgs &&... args) :
        repo_(std::forward<TRepoArgs>(args)...)
{ }

template<typename TRepository>
template<typename TMetricType>
registered_metric<TMetricType>& metrics_registry<TRepository>::get(const metric_path& path)
{
    static const std::string mtype = (TMetricType()).metric_type();
    auto& l = repo_.get_or_add(path, [tn = mtype]() { return std::make_unique<registered_metric<TMetricType>>(tn); });

    if (l.type() != mtype)
        throw metric_type_mismatch(l.type(), mtype);

    return static_cast<registered_metric<TMetricType>&>(l);
}

template<typename TRepository>
template<typename TMetricType>
TMetricType& metrics_registry<TRepository>::get(const metric_path& path, const tag_collection& tags)
{
    auto& r = get<TMetricType>(path);
    return r.template tagged<TMetricType>(tags);
}

template<typename TRepository>
template<typename THandler>
void metrics_registry<TRepository>::visit_registered_metrics(THandler &&handler)
{
    repo_.visit(std::forward<THandler>(handler));
}

template<typename TRepository>
template<typename TCount>
counter<TCount>& metrics_registry<TRepository>::counter(const metric_path& name, const tag_collection& tags)
{
    return get<cxxmetrics::counter<TCount>>(name, tags);
}

}

#endif //CXXMETRICS_METRICS_REGISTRY_HPP
