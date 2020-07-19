#ifndef CXXMETRICS_METRICS_REGISTRY_HPP
#define CXXMETRICS_METRICS_REGISTRY_HPP

// TODO: use shared_mutexes with C++17
#include <mutex>
#include <memory>
#include "publisher.hpp"
#include "tag_collection.hpp"
#include "counter.hpp"
#include "ewma.hpp"
#include "gauge.hpp"
#include "histogram.hpp"
#include "meter.hpp"
#include "timer.hpp"

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

    class bound_visitor
    {
        const TVisitor& visitor_;
        const tag_collection& tags_;
    public:
        bound_visitor(const TVisitor& v, const tag_collection& t) :
            visitor_(v),
            tags_(t)
        { }

        template<typename T>
        decltype(std::declval<TVisitor>()(std::declval<const tag_collection>(), std::declval<T>()))
        operator()(T&& arg) const
        {
            return visitor_(tags_, std::forward<T>(arg));
        }
    };
public:
    invokable_snapshot_visitor_builder(TVisitor&& visitor) :
            visitor_(std::forward<TVisitor>(visitor))
    { }

    std::size_t visitor_size() const override
    {
        return sizeof(invokable_snapshot_visitor<bound_visitor>);
    }

    void construct(snapshot_visitor* location, const tag_collection& collection) override
    {
        using namespace std::placeholders;
        new (location) invokable_snapshot_visitor<bound_visitor>(bound_visitor(visitor_, collection));
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

    std::unordered_map<std::string, std::unique_ptr<basic_publish_options>> pubdata_;
    mutable std::mutex pubdatalock_;

    template<typename TMetricType, typename... TConstructorArgs>
    std::shared_ptr<TMetricType> tagged(const tag_collection& tags, TConstructorArgs&&... args)
    {
        auto builder = [&]() -> std::shared_ptr<TMetricType> {
            return std::make_shared<TMetricType>(std::forward<TConstructorArgs>(args)...);
        };

        invokable_metric_builder<typename std::decay<decltype(builder)>::type> metricbuilder(std::move(builder));
        return std::static_pointer_cast<TMetricType>(this->child(tags, &metricbuilder));
    }

    template<typename TMetricType>
    bool add_existing(const tag_collection& tags, std::shared_ptr<TMetricType>&& metric)
    {
        bool added = false;
        auto builder = [&]() -> std::shared_ptr<TMetricType> {
            added = true;
            return std::move(metric);
        };

        invokable_metric_builder<typename std::decay<decltype(builder)>::type> metricbuilder(std::move(builder));
        std::static_pointer_cast<TMetricType>(this->child(tags, &metricbuilder));

        return added;
    }

    template<typename TDataType, typename... TConstructArgs>
    typename std::enable_if<std::is_base_of<basic_publish_options, TDataType>::value, TDataType>::type&
    get_or_create_publish_data(TConstructArgs&&... args)
    {
        std::lock_guard<std::mutex> lock(pubdatalock_);
        auto key = ctti::nameof<TDataType>().str();
        auto& ptr = pubdata_[key];

        if (!ptr)
            ptr = std::make_unique<TDataType>(std::forward<TConstructArgs>(args)...);

        return static_cast<TDataType&>(*ptr);
    }

    template<typename TDataType>
    typename std::enable_if<std::is_base_of<basic_publish_options, TDataType>::value, TDataType>::type*
    try_get_publish_data() const
    {
        std::lock_guard<std::mutex> lock(pubdatalock_);
        auto fnd = pubdata_.find(ctti::nameof<TDataType>().str());
        if (fnd == pubdata_.end())
            return nullptr;

        return static_cast<TDataType*>(fnd->second.get());
    }

    template<typename TRepository>
    friend class metrics_registry;
    template<typename TRepository>
    friend class metrics_publisher;
protected:
    template<typename TMetricType>
    struct basic_metric_builder
    {
        virtual ~basic_metric_builder() = default;
        virtual std::shared_ptr<TMetricType> build() = 0;
    };

    template<typename TBuilder>
    class invokable_metric_builder final : public basic_metric_builder<typename decltype(std::declval<TBuilder>()())::element_type>
    {
        TBuilder builder_;
    public:
        invokable_metric_builder(TBuilder&& builder) :
                builder_(std::forward<TBuilder>(builder))
        { }

        decltype(builder_()) build() override
        {
            return builder_();
        }
    };

    virtual void visit_each(internal::registered_snapshot_visitor_builder& builder) = 0;
    virtual void aggregate_all(snapshot_visitor& visitor) = 0;
    virtual std::shared_ptr<internal::metric> child(const tag_collection& tags, void* metricbuilder) = 0;

public:
    basic_registered_metric(const std::string& type) :
            type_(type)
    { }

    virtual ~basic_registered_metric() = default;

    /**
     * \brief Visits all of the metrics with their tag values, calling a handler for each
     *
     * The handler should accept 2 arguments: the first is a tag_collection, which will be the
     * tags associated to the metric. The second will be the actual metric snapshot value
     *
     * \tparam THandler the handler type which ought to be auto-deduced from the parameter
     * \param handler the instance of the handler which will be called for each of the metrics
     */
    template<typename THandler>
    void visit(THandler&& handler) {
        internal::invokable_snapshot_visitor_builder<THandler> builder(std::forward<THandler>(handler));
        this->visit_each(builder);
    }

    /**
     * \brief Aggregates all of the metrics and their different tag values into a single metric
     *
     * The Handler will be called with the aggregated snapshot of all the tagged permutations of the metric
     *
     * \tparam THandler the visitor handler that will receive a single call with the aggregated snapshot
     *
     * \param handler the instance of the handler
     */
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
 * \tparam TMetricType the type of metric registered in the repository
 */
template<typename TMetricType>
class registered_metric : public basic_registered_metric
{
    std::unordered_map<tag_collection, std::shared_ptr<TMetricType>> metrics_;
    std::mutex lock_;

protected:
    void visit_each(internal::registered_snapshot_visitor_builder& builder) override;
    void aggregate_all(snapshot_visitor& visitor) override;
    std::shared_ptr<internal::metric> child(const tag_collection& tags, void* metricbuilder) override;

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
            loc->visit(p.second->snapshot());
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

    auto result = itr->second->snapshot();
    for (++itr; itr != metrics_.end(); ++itr)
    {
        result.merge(itr->second->snapshot());
    }

    lock.unlock();
    visitor.visit(result);
}

template<typename TMetricType>
std::shared_ptr<internal::metric> registered_metric<TMetricType>::child(const cxxmetrics::tag_collection &tags, void* metricbuilder)
{
    std::lock_guard<std::mutex> lock(lock_);
    auto res = metrics_.find(tags);

    if (res != metrics_.end())
        return std::static_pointer_cast<internal::metric>(res->second);

    return std::static_pointer_cast<internal::metric>(
            metrics_.emplace(tags, static_cast<basic_metric_builder<TMetricType>*>(metricbuilder)->build()).first->second);
}

/**
 * \brief The default metric repository that registers metrics in a standard unordered map with a mutex lock
 */
template<typename TAlloc = std::allocator<char>>
class basic_default_repository
{
    template<typename First, typename Second>
    using pointer_allocator_type = typename std::allocator_traits<TAlloc>::template rebind_alloc<std::pair<std::add_const_t<First>, std::unique_ptr<Second>>>;

    std::unordered_map<metric_path, std::unique_ptr<basic_registered_metric>, std::hash<metric_path>, std::equal_to<metric_path>, pointer_allocator_type<metric_path, basic_registered_metric>> metrics_;
    std::unordered_map<std::string, std::unique_ptr<basic_publish_options>, std::hash<std::string>, std::equal_to<std::string>, pointer_allocator_type<std::string, basic_publish_options>> data_;

    mutable std::mutex metriclock_;
    mutable std::mutex datalock_;

public:
    basic_default_repository() = default;

    template<typename TMetricPtrBuilder>
    basic_registered_metric& get_or_add(const metric_path& name, const TMetricPtrBuilder& builder);
    basic_registered_metric* get(const metric_path& name);

    template<typename THandler>
    void visit(THandler&& handler);

    constexpr const tag_collection& tags(const tag_collection& tags) const noexcept { return tags; }

    template<typename TDataType, typename... TConstructArgs>
    typename std::enable_if<std::is_base_of<basic_publish_options, TDataType>::value, TDataType>::type& get_publish_data(TConstructArgs&&... args);

    template<typename TDataType>
    typename std::enable_if<std::is_base_of<basic_publish_options, TDataType>::value, TDataType>::type* get_publish_data() const;
};

template<typename TAlloc>
template<typename TMetricPtrBuilder>
basic_registered_metric& basic_default_repository<TAlloc>::get_or_add(const metric_path& name, const TMetricPtrBuilder& builder)
{
    std::lock_guard<std::mutex> lock(metriclock_);
    auto existing = metrics_.find(name);

    if (existing == metrics_.end())
    {
        auto ptr = builder();
        return *metrics_.emplace(name, std::move(ptr)).first->second;
    }

    return *existing->second;
}

template<typename TAlloc>
basic_registered_metric* basic_default_repository<TAlloc>::get(const metric_path& name)
{
    std::lock_guard<std::mutex> lock(metriclock_);
    auto existing = metrics_.find(name);

    if (existing == metrics_.end())
        return nullptr;

    return existing->second.get();
}

template<typename TAlloc>
template<typename THandler>
void basic_default_repository<TAlloc>::visit(THandler&& handler)
{
    std::lock_guard<std::mutex> lock(metriclock_);
    for (auto& pair : metrics_)
        handler(pair.first, *pair.second);
}

template<typename TAlloc>
template<typename TDataType, typename... TConstructArgs>
typename std::enable_if<std::is_base_of<basic_publish_options, TDataType>::value, TDataType>::type&
basic_default_repository<TAlloc>::get_publish_data(TConstructArgs&&... args)
{
    std::lock_guard<std::mutex> lock(datalock_);
    auto key = ctti::nameof<TDataType>().str();
    auto& ptr = data_[key];

    if (!ptr)
        ptr = std::make_unique<TDataType>(std::forward<TConstructArgs>(args)...);

    return static_cast<TDataType&>(*ptr);
}

template<typename TAlloc>
template<typename TDataType>
typename std::enable_if<std::is_base_of<basic_publish_options, TDataType>::value, TDataType>::type*
basic_default_repository<TAlloc>::get_publish_data() const
{
    std::lock_guard<std::mutex> lock(datalock_);
    auto key = ctti::nameof<TDataType>().str();
    auto fnd = data_.find(ctti::nameof<TDataType>().str());
    if (fnd == data_.end())
        return nullptr;

    return static_cast<TDataType*>(fnd->second.get());
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

    template<typename TMetricType, typename... TConstructorArgs>
    std::shared_ptr<TMetricType> get(const metric_path& path, const tag_collection& tags, TConstructorArgs&&... args);

    template<typename TDataType, typename... TConstructArgs>
    TDataType& get_publish_data(TConstructArgs&&... args) { return repo_.template get_publish_data<TDataType>(std::forward<TConstructArgs>(args)...); }

    auto* try_get(const metric_path& name) { return repo_.get(name); }

    friend class metrics_publisher<TRepository>;
public:
    using repository_type = TRepository;

    /**
     * \brief Construct the registry with the arguments being passed to the underlying repository
     */
    template<typename... TRepoArgs>
    metrics_registry(TRepoArgs&&... args);

    metrics_registry(const metrics_registry&) = default;
    metrics_registry(metrics_registry&& other) noexcept :
            repo_(std::move(other.repo_))
    { }
    ~metrics_registry() = default;

    /**
     * \brief Get the repository wide publish options
     */
    const cxxmetrics::publish_options& publish_options() const;

    /**
     * \brief Set the repository wide publish options
     *
     * \note individual metrics may override these
     *
     * \param options the new options for the repository
     */
    void publish_options(cxxmetrics::publish_options&& options);

    /**
     * \brief Set the publish option overrides for a single metric
     *
     * if the metric provided isn't registered, this method does nothing
     *
     * \param name the metric on which to set the options
     * \param options the new options for the metric
     */
    void publish_options(const metric_path& name, cxxmetrics::publish_options&& options);

    /**
     *  \brief Run a visitor on all of the registered metrics
     *
     *  This is particularly useful for metric publishers. The handler will be called with
     *  a signature of
     *
     *  handler(const metric_path&, basic_registered_metric&)
     *
     *  The basic_registered_metric is where publishers can get metric-specific publisher
     *  data and where you can use another handler to either aggregate the metric values
     *  across all sets of tags or visit each of the tagged permutations of the metric.
     *
     * \param handler the handler to execute per metric registration
     */
    template<typename THandler>
    void visit_registered_metrics(THandler&& handler);

    /**
     * \brief Register an existing metric in the registry (perhaps one obtained from another registry)
     *
     * \throws metric_type_mismatch if there is already a registered metric at the path of a different type
     *
     * \tparam TMetric the metric type to register
     *
     * \param name the name of the metric to register
     * \param metric the metric to register
     * \param tags the tags for the permutation being registered
     *
     * \return true if the metric was registered at the path or false if there was already a metric there
     */
    template<typename TMetric>
    bool register_existing(const metric_path& name, std::shared_ptr<TMetric> metric, const tag_collection& tags = tag_collection());

    /**
     * \brief Get the registered counter or register a new one with the given path and tags
     *
     * \throws metric_type_mismatch if there is already a registered metric at the path of a different type
     *
     * \tparam TCount the type of counter
     *
     * \param name the name of the metric to get
     * \param initialValue the initial value for the counter (ignored if counter already exists)
     * \param tags the tags for the permutation being sought
     *
     * \return the counter at the path specified with the tags specified
     */
    template<typename TCount = int64_t>
    std::shared_ptr<cxxmetrics::counter<TCount>> counter(const metric_path& name, TCount&& initialValue, const tag_collection& tags = tag_collection());

    /**
     * \brief Another overload for getting a counter without an initial value
     */
    template<typename TCount = int64_t>
    std::shared_ptr<cxxmetrics::counter<TCount>> counter(const metric_path& name, const tag_collection& tags = tag_collection())
    {
        return this->template counter<TCount>(name, 0, tags);
    }

    /**
     * \brief Get the registered exponential moving average or register a new one with the given path and tags
     *
     * \throws metric_type_mismatch if there is already a registered metric at the path of a different type
     *
     * \tparam Window the window over which the ewma should decay
     * \tparam Interval the interval size within window for the value
     * \tparam TValue the type of data in the ewma
     *
     * \param name the name of the metric to get
     * \param window the ewma full window outside of which values are fully decayed (ignored if the ewma already exists)
     * \param interval the window in which values are summed (ignored if the ewma already exists)
     * \param tags the tags for the permutation being sought.
     *
     * \return the ewma at the path specified with the tags specified
     */
    template<period::value Window, period::value Interval = time::seconds(1), typename TValue = double>
    std::shared_ptr<cxxmetrics::ewma<Window, Interval, TValue>> ewma(const metric_path& name,
                       const tag_collection& tags = tag_collection());

    /**
     * \brief Get the registered gauge or register a new one with the given path and tags
     *
     * \throws metric_type_mismatch if there is already a registered metric at the path of a different type
     *
     * \tparam TGaugeType the type of data provider for the gauge
     * \tparam TAggregation the way to aggregate the results of the same gauge for different tags (sum or avg)
     *
     * \param name the name of the metric to get
     * \param data_provider the instance of the gauge type that will supply the gauge with data
     * \param tags the tags for the permutation being sought
     *
     * \return the gauge at the path specified with the tags specified
     */
    template<typename TGaugeType, gauges::gauge_aggregation_type TAggregation = gauges::aggregation_average>
    std::shared_ptr<cxxmetrics::gauge<TGaugeType, TAggregation>> gauge(const metric_path& name,
            TGaugeType&& data_provider,
            const tag_collection& tags = tag_collection());

    /**
     * \brief Get the registered histogram or register a new one with the given path and tags
     *
     * \throws metric_type_mismatch if there is already a registered metric at the path of a different type, including a different reservoir
     *
     * \tparam TReservoir the reservoir type
     *
     * \param name the name of the metric to get
     * \param reservoir the reservoir instance under the histogram
     * \param tags the tags for the permutation being sought
     *
     * \return the histogram at the path specified with the tags specified
     */
    template<typename TReservoir = uniform_reservoir<int64_t, 1024>>
    std::shared_ptr<cxxmetrics::histogram<typename TReservoir::value_type, TReservoir>> histogram(const metric_path& name,
            TReservoir&& reservoir = TReservoir(),
            const tag_collection& tags = tag_collection());

    /**
     * \brief Get the registered meter or register a new one with the given path and tags
     *
     * \throws metric_type_mismatch if there is already a registered metric at the path of a different type, including different time windows (independent of parameter order)
     *
     * \tparam Interval the interval to track in each of the time windows
     * \tparam TWindows the windows over which the meter tracks
     *
     * \param name the name of the metric to get
     * \param tags the tags for the permutation being sought
     *
     * \return the meter at the path specified with the tags specified
     */
    template<period::value Interval, period::value... TWindows>
    std::shared_ptr<cxxmetrics::meter<Interval, TWindows...>> meter(const metric_path& name,
            const tag_collection& tags = tag_collection());

    /**
     * \brief Get the registered timer or register a new one with the given path and tags
     *
     * \throws metric_type_mismatch if there is already a registered metric at the path of a different type, including different time windows and reservoirs (independent of parameter order)
     *
     * \tparam TRateInterval the interval over which to track rates
     * \tparam TClock the clock to use for tracking time
     * \tparam TReservoir the type of reservoir to use for timing metrics, this should be a reservoir of TClock::duration values
     * \tparam TRateWindows the windows over which to track the rate of timed calls
     *
     * \param name the name of the metric to get
     * \param reservoir the reservoir instance
     * \param tags the tags for the permutation being sought
     *
     * \return the timer at the path specified with the tags specified
     */
    template<period::value TRateInterval, typename TClock = std::chrono::steady_clock, typename TReservoir = uniform_reservoir<typename TClock::duration, 1024>, period::value... TRateWindows>
    std::shared_ptr<cxxmetrics::timer<TRateInterval, TClock, TReservoir, TRateWindows...>> timer(const metric_path& name,
            TReservoir&& reservoir = TReservoir(),
            const tag_collection& tags = tag_collection());

#if __cplusplus >= 201700

    /**
     * A wrapper for timer with some sensible defaults
     *
     * \throws metric_type_mismatch if there is already a registered metric at the path of a different type, including different time windows and reservoirs (independent of parameter order)
     *
     * \tparam TRateInterval the interval over which to track rates
     * \tparam TReservoir the type of reservoir to use
     * \tparam TReservoirSize the size of the reservoir
     * \tparam TRateWindows the windows over which to track the rate of timed calls
     */
    template<period::value TRateInterval = time::seconds(1), template<typename, std::size_t> typename TReservoir = uniform_reservoir, std::size_t TSize = 1024, period::value... TRateWindows>
    std::shared_ptr<cxxmetrics::timer<TRateInterval, std::chrono::steady_clock, TReservoir<typename std::chrono::steady_clock::duration, TSize>, TRateWindows...>>
    timer(const metric_path& name, const tag_collection& tags = tag_collection())
    {
        return this->template timer<TRateInterval, std::chrono::steady_clock, TReservoir<typename std::chrono::steady_clock::duration, TSize>, TRateWindows...>(name, TReservoir<typename std::chrono::steady_clock::duration, TSize>(), tags);
    }
#endif

};

template<typename TRepository>
template<typename... TRepoArgs>
metrics_registry<TRepository>::metrics_registry(TRepoArgs &&... args) :
        repo_(std::forward<TRepoArgs>(args)...)
{ }

template<typename TRepository>
const cxxmetrics::publish_options& metrics_registry<TRepository>::publish_options() const
{
    static const cxxmetrics::publish_options defaultopts;
    auto existing = repo_.template get_publish_data<cxxmetrics::publish_options>();
    if (existing == nullptr)
        return defaultopts;

    return *existing;
}

template<typename TRepository>
void metrics_registry<TRepository>::publish_options(cxxmetrics::publish_options&& options)
{
    repo_.template get_publish_data<cxxmetrics::publish_options>() = std::move(options);
}

template<typename TRepository>
void metrics_registry<TRepository>::publish_options(const metric_path& path, cxxmetrics::publish_options&& options)
{
    auto l = try_get(path);
    if (l == nullptr)
        return;

    l->template get_or_create_publish_data<cxxmetrics::publish_options>() = std::move(options);
}

template<typename TRepository>
template<typename TMetricType>
registered_metric<TMetricType>& metrics_registry<TRepository>::get(const metric_path& path)
{
    static const std::string mtype = internal::metric_default_value<TMetricType>().metric_type();
    auto& l = repo_.get_or_add(path, [tn = mtype]() { return std::make_unique<registered_metric<TMetricType>>(tn); });

    if (l.type() != mtype)
        throw metric_type_mismatch(l.type(), mtype);

    return static_cast<registered_metric<TMetricType>&>(l);
}

template<typename TRepository>
template<typename TMetricType, typename... TConstructorArgs>
std::shared_ptr<TMetricType> metrics_registry<TRepository>::get(const metric_path& path, const tag_collection& tags, TConstructorArgs&&... args)
{
    auto& r = get<TMetricType>(path);
    return r.template tagged<TMetricType>(repo_.tags(tags), std::forward<TConstructorArgs>(args)...);
}

template<typename TRepository>
template<typename THandler>
void metrics_registry<TRepository>::visit_registered_metrics(THandler &&handler)
{
    repo_.visit(std::forward<THandler>(handler));
}

template<typename TRepository>
template<typename TMetric>
bool metrics_registry<TRepository>::register_existing(const metric_path& name,
        std::shared_ptr<TMetric> metric,
        const tag_collection& tags)
{
    if (!metric)
        return false;

    auto& l = repo_.get_or_add(name, [tn = metric->metric_type()]() { return std::make_unique<registered_metric<TMetric>>(tn); });
    if (l.type() != metric->metric_type())
        throw metric_type_mismatch(l.type(), metric->metric_type());

    return l.template add_existing<TMetric>(repo_.tags(tags), std::move(metric));
}

template<typename TRepository>
template<typename TCount>
std::shared_ptr<cxxmetrics::counter<TCount>> metrics_registry<TRepository>::counter(const metric_path& name,
        TCount&& initialValue,
        const tag_collection& tags)
{
    return get<cxxmetrics::counter<TCount>>(name, tags, std::forward<TCount>(initialValue));
}

template<typename TRepository>
template<period::value Window, period::value Interval, typename TValue>
std::shared_ptr<cxxmetrics::ewma<Window, Interval, TValue>> metrics_registry<TRepository>::ewma(const metric_path& name,
        const tag_collection& tags)
{
    return get<cxxmetrics::ewma<Window, Interval, TValue>>(name, tags);
}

template<typename TRepository>
template<typename TGaugeType, gauges::gauge_aggregation_type TAggregation>
std::shared_ptr<cxxmetrics::gauge<TGaugeType, TAggregation>> metrics_registry<TRepository>::gauge(
        const metric_path& name,
        TGaugeType&& data_provider,
        const tag_collection& tags)
{
    return get<cxxmetrics::gauge<TGaugeType, TAggregation>>(name, tags, std::forward<TGaugeType>(data_provider));
}

template<typename TRepository>
template<typename TReservoir>
std::shared_ptr<cxxmetrics::histogram<typename TReservoir::value_type, TReservoir>> metrics_registry<TRepository>::histogram(const metric_path& name,
        TReservoir&& reservoir,
        const tag_collection& tags)
{
    return get<cxxmetrics::histogram<typename TReservoir::value_type, TReservoir>>(name, tags, std::forward<TReservoir>(reservoir));
}

template<typename TRepository>
template<period::value Interval, period::value... TWindows>
std::shared_ptr<cxxmetrics::meter<Interval, TWindows...>> metrics_registry<TRepository>::meter(const metric_path& name,
        const tag_collection& tags)
{
    return get<cxxmetrics::meter<Interval, TWindows...>>(name, tags);
}

template<typename TRepository>
template<period::value TRateInterval, typename TClock, typename TReservoir, period::value... TRateWindows>
std::shared_ptr<cxxmetrics::timer<TRateInterval, TClock, TReservoir, TRateWindows...>> metrics_registry<TRepository>::timer(const metric_path& name,
        TReservoir&& reservoir,
        const tag_collection& tags)
{
    return get<cxxmetrics::timer<TRateInterval, TClock, TReservoir, TRateWindows...>>(name, tags, std::forward<TReservoir>(reservoir));
}

}

#include "publisher_impl.hpp"

#endif //CXXMETRICS_METRICS_REGISTRY_HPP
