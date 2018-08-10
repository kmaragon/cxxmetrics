#ifndef CXXMETRICS_PUBLISHER_HPP
#define CXXMETRICS_PUBLISHER_HPP

#include <memory>
#include "meta.hpp"
#include "snapshots.hpp"
#include "metric_path.hpp"

namespace cxxmetrics
{

// fwd declaration
template<typename TRepository>
class metrics_registry;
class basic_registered_metric;

class scale_factor
{
    double factor_;
    bool apply_;
public:
    constexpr scale_factor() noexcept : factor_(1), apply_(false) { }
    constexpr scale_factor(double factor) noexcept : factor_(factor), apply_(true) { }
    constexpr operator bool() const noexcept { return apply_; }
    constexpr double factor() const noexcept { return factor_; }
};

/**
 * \brief the base type that all publish options need to implement
 */
class basic_publish_options
{
public:
    virtual ~basic_publish_options() = default;
};

/**
 * \brief Options to apply to value types while publishing
 */
class value_publish_options
{
    scale_factor scale_;
public:
    constexpr value_publish_options(const value_publish_options&) noexcept = default;
    constexpr value_publish_options& operator=(const value_publish_options&) noexcept = default;

    constexpr value_publish_options(const scale_factor& sf = scale_factor()) noexcept :
            scale_(sf)
    { }

    /**
     * \brief a scaling factor to the metric, if set
     */
    constexpr scale_factor scale() const noexcept { return scale_; }
};

/**
 * \brief Options to apply to meter types while publishing
 */
class meter_publish_options : public virtual value_publish_options
{
    bool mean_;
public:
    meter_publish_options(const scale_factor& sf = scale_factor()) :
            value_publish_options(sf),
            mean_(true)
    { }
    meter_publish_options(bool with_mean, const scale_factor& sf = scale_factor()) :
            value_publish_options(sf), mean_(with_mean)
    { }

    meter_publish_options(const meter_publish_options& other) noexcept :
            value_publish_options(other),
            mean_(other.mean_)
    { }

    meter_publish_options& operator=(const meter_publish_options& other) noexcept = default;

    /**
     * \brief whether or not the publisher should publish the mean
     */
    bool include_mean() const noexcept { return mean_; }
};

/**
 * \brief a visitor that can be used with quantile_options if honoring custom quantiles for histogram publishing
 */
class quantile_visitor
{
public:
    virtual void visit(const quantile& q, metric_value&& value) const = 0;
    virtual ~quantile_visitor() = default;
};

class basic_quantile_options
{
    template<typename THandler>
    class invokable_quantile_visitor : public quantile_visitor
    {
        THandler handler_;
    public:
        invokable_quantile_visitor(THandler&& handler) :
                handler_(std::forward<THandler>(handler))
        { }

        void visit(const quantile& q, metric_value&& value) const override
        {
            handler_(q, std::move(value));
        }
    };

public:
    template<typename THandler, typename = decltype(std::declval<THandler>()(std::declval<quantile>(), std::declval<metric_value>()))>
    void visit(const histogram_snapshot& snapshot, THandler&& hnd) const
    {
        invokable_quantile_visitor<THandler> visitor(std::forward<THandler>(hnd));
        visit(snapshot, visitor);
    }

    virtual void visit(const histogram_snapshot& snapshot, const quantile_visitor& visitor) const = 0;
    virtual ~basic_quantile_options() = default;
};

namespace quantiles
{

/**
 * \brief Quantile options that allow one to specify quantiles from a histogram
 *
 * \tparam TQuantiles the quantiles that should be published
 */
template<quantile::value... TQuantiles>
class processed_quantile_options : public basic_quantile_options
{
    template<quantile::value... _Quantiles>
    struct visit_one;

    template<quantile::value TVisit, quantile::value... TRemaining>
    struct visit_one<TVisit, TRemaining...>
    {
        void operator()(const histogram_snapshot& snapshot, const quantile_visitor& visitor) const
        {
            visitor.visit(TVisit, snapshot.value<TVisit>());
            visit_one<TRemaining...> next;
            next(snapshot, visitor);
        }
    };

    template<quantile::value _Quantile>
    struct visit_one<_Quantile>
    {
        void operator()(const histogram_snapshot& snapshot, const quantile_visitor& visitor) const
        {
            visitor.visit(_Quantile, snapshot.value<_Quantile>());
        }
    };
public:
    /**
     * \brief collect the requested quantiles in the snapshot, calling the visitor for each
     */
    void visit(const histogram_snapshot &snapshot, const quantile_visitor &visitor) const override
    {
        visit_one<TQuantiles...> fn;
        fn(snapshot, visitor);
    }
};

template<typename TQuantiles>
struct quantile_options_builder;

template<period::value... TQuantiles>
struct quantile_options_builder<templates::sortable_template_collection<TQuantiles...>> : public processed_quantile_options<TQuantiles...>
{ };

}

/**
 * \brief The options for the quantiles that should be published
 *
 * As with all options, this may or may not be supported by the publisher
 *
 * \tparam TQuantiles the quantiles to of the histogram that should be reported
 */
template<quantile::value... TQuantiles>
class quantile_options : public quantiles::quantile_options_builder<typename templates::sort_unique<TQuantiles...>::type>
{ };

/**
 * \brief Options to apply to histogram types while publishing
 */
class histogram_publish_options : public virtual value_publish_options
{
    static basic_quantile_options& default_quantiles()
    {
        static auto def = std::make_unique<quantile_options<quantile(50.0l), quantile(90.0l), quantile(99.0l)>>();
        return *def;
    }
    std::unique_ptr<basic_quantile_options> quantiles_;
    bool count_;
public:
    histogram_publish_options(histogram_publish_options&& other) noexcept :
            value_publish_options(std::move(other)),
            quantiles_(std::move(other.quantiles_)),
            count_(other.count_)
    { }

    histogram_publish_options(const scale_factor& sf = scale_factor()) noexcept :
            value_publish_options(sf),
            count_(true)
    { }

    histogram_publish_options(bool publish_count, const scale_factor& sf = scale_factor()) noexcept :
            value_publish_options(sf),
            count_(publish_count)
    { }

    template<typename TQuantileOptions, typename = typename std::enable_if<std::is_base_of<basic_quantile_options, TQuantileOptions>::value, void>::type>
    histogram_publish_options(TQuantileOptions&& quantile_options, bool publish_count = true, const scale_factor& sf = scale_factor()) :
            value_publish_options(sf),
            quantiles_(std::make_unique<TQuantileOptions>(std::forward<TQuantileOptions>(quantile_options))),
            count_(publish_count)
    { }

    histogram_publish_options& operator=(histogram_publish_options&& other) noexcept
    {
        value_publish_options::operator=(std::move(other));
        quantiles_ = std::move(other.quantiles_);
        count_ = other.count_;
        return *this;
    }

    /**
     * \brief Get the quantiles that should be published
     *
     * \return the quantile options for the publisher, will either be options specific or universal default
     */
    basic_quantile_options& quantiles() const noexcept
    {
        if (quantiles_)
            return *quantiles_;
        return default_quantiles();
    }

    /**
     * \brief whether or not the histogram total should be published
     */
    bool include_count() const noexcept {
        return count_;
    }
};

/**
 * \brief Options to apply to timer types while publishing
 */
class timer_publish_options : public histogram_publish_options, public meter_publish_options
{
    bool rates_;
public:
    timer_publish_options(timer_publish_options&& other) noexcept :
            histogram_publish_options(std::move(other)),
            meter_publish_options(std::move(other)),
            rates_(other.rates_)
    { }

    timer_publish_options(bool rates = true, const scale_factor& sf = scale_factor()) noexcept :
            histogram_publish_options(sf),
            meter_publish_options(sf),
            rates_(rates)
    { }

    timer_publish_options(bool rates, bool publish_count, const scale_factor& sf = scale_factor()) noexcept :
            histogram_publish_options(publish_count, sf),
            meter_publish_options(sf),
            rates_(rates)
    { }

    timer_publish_options(bool rates, bool publish_count, bool publish_mean, const scale_factor& sf = scale_factor()) noexcept :
            histogram_publish_options(publish_count, sf),
            meter_publish_options(publish_mean, sf),
            rates_(rates)
    { }

    template<typename TQuantileOptions, typename = typename std::enable_if<std::is_base_of<basic_quantile_options, TQuantileOptions>::value, void>::type>
    timer_publish_options(TQuantileOptions&& quantile_options, bool rates, bool publish_count = true, const scale_factor& sf = scale_factor()) :
            histogram_publish_options(quantile_options, publish_count, sf),
            meter_publish_options(sf),
            rates_(rates)
    { }


    template<typename TQuantileOptions, typename = typename std::enable_if<std::is_base_of<basic_quantile_options, TQuantileOptions>::value, void>::type>
    timer_publish_options(TQuantileOptions&& quantile_options, bool rates, bool publish_count, bool publish_mean, const scale_factor& sf = scale_factor()) :
            histogram_publish_options(quantile_options, publish_count, sf),
            meter_publish_options(publish_mean, sf),
            rates_(rates)
    { }

    timer_publish_options& operator=(timer_publish_options&& other) noexcept
    {
        histogram_publish_options::operator=(std::move(other));
        meter_publish_options::operator=(std::move(other));
        return *this;
    }

    /**
     * \brief Whether or not the rates should be included in the timer data
     */
    bool include_rates() const
    {
        return rates_;
    }
};

class publish_options : public basic_publish_options
{
    value_publish_options values_;
    meter_publish_options meters_;
    histogram_publish_options histograms_;
    timer_publish_options timers_;
public:
    publish_options(publish_options&& other) noexcept :
            values_(std::move(other.values_)),
            meters_(std::move(other.meters_)),
            histograms_(std::move(other.histograms_)),
            timers_(std::move(other.timers_))
    { }

    publish_options(
            value_publish_options&& value_options = value_publish_options(),
            meter_publish_options&& meter_options = meter_publish_options(),
            histogram_publish_options&& histogram_options = histogram_publish_options(),
            timer_publish_options&& timer_options = timer_publish_options()) :
            values_(std::move(value_options)),
            meters_(std::move(meter_options)),
            histograms_(std::move(histogram_options)),
            timers_(std::move(timer_options))
    { }

    publish_options(
            meter_publish_options&& meter_options,
            histogram_publish_options&& histogram_options = histogram_publish_options(),
            timer_publish_options&& timer_options = timer_publish_options()) :
            meters_(std::move(meter_options)),
            histograms_(std::move(histogram_options)),
            timers_(std::move(timer_options))
    { }

    publish_options(
            histogram_publish_options&& histogram_options,
            timer_publish_options&& timer_options = timer_publish_options()) :
            histograms_(std::move(histogram_options)),
            timers_(std::move(timer_options))
    { }

    publish_options(
            timer_publish_options&& timer_options) :
            timers_(std::move(timer_options))
    { }

    publish_options& operator=(publish_options&& other) noexcept
    {
        values_ = std::move(other.values_);
        meters_ = std::move(other.meters_);
        histograms_ = std::move(other.histograms_);
        timers_ = std::move(other.timers_);

        return *this;
    }

    const value_publish_options& value_options() const noexcept { return values_; };
    const meter_publish_options& meter_options() const noexcept { return meters_; };
    const histogram_publish_options& histogram_options() const noexcept { return histograms_; };
    const timer_publish_options& timer_options() const noexcept { return timers_; };

};

/**
 * \brief The base class for the metrics publisher
 *
 * A publisher simply uses the standard visit pattern to access metrics and publish.
 * However, inheriting from this class allows the publisher to gain additional insight into
 * metrics that are registered in the underlying registry as well as inject it's own data
 * at various levels
 *
 * \tparam TMetricRepo the repository being used by the registry
 */
template<typename TMetricRepo>
class metrics_publisher
{
    metrics_registry<TMetricRepo>& registry_;

protected:

    /**
     * \brief Get the publish options for a metric found in the repository
     *
     * Publish options are a special global set of options that tell publishers how to publish metrics
     *
     * They are effectively the same thing as any other custom data but are codified into cxxmetrics as
     * a way to set some semblance of a standard across publishers for obvious things.
     *
     * \return The publish options for the repository
     */
    const publish_options& effective_options(basic_registered_metric& metric) const;

    /**
     * \brief Get a piece of data from the registry. Generally, this will be a type specific to the publisher
     *
     * This allows the publisher to store data in the registry that is specific to the publisher. If the data is
     * already registered, the returned value will be the registered data. Otherwise, the object is constructed
     * and stored in the registry using the arguments specified. The data, however, must be a subclass of
     * \refitem basic_publish_options
     *
     * \tparam TDataType the type of data to get from the registry
     * \tparam TBuildArgs the types of arguments that will be used to construct the data if it doesn't exist
     *
     * \return the registered data of the requested type
     */
    template<typename TDataType, typename... TBuildArgs>
    typename std::enable_if<std::is_base_of<basic_publish_options, TDataType>::value, TDataType>::type&
    get_data(TBuildArgs&&... args);

    /**
     * \brief Get a piece of data from the registry for a specific metric by name.
     *
     * \warning Don't use this in a visitor - use the overload that operates directly on the metric instead on the metric the visitor receives. Ignoring this warning will result in deadlock
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
    typename std::enable_if<std::is_base_of<basic_publish_options, TDataType>::value, TDataType>::type*
    get_data_for(const metric_path& path, TBuildArgs&&... args);

    /**
     * \brief Get a piece of data from the registry for a specific metric by reference.
     *
     * \tparam TDataType the type of data to attach to the metric
     * \tparam TBuildArgs The types of arguments that will be used to construct the data if it doesn't exist but the path does
     *
     * \param metric the metric to attach the data to or get the attached data
     *
     * \return the attached data
     */
    template<typename TDataType, typename... TBuildArgs>
    typename std::enable_if<std::is_base_of<basic_publish_options, TDataType>::value, TDataType>::type&
    get_data_for(basic_registered_metric& metric, TBuildArgs&&... args);

    /**
     * \brief Get whether or not the custom data exists on the specified metric path
     *
     * \warning Don't use this in a visitor - use the overload that operates directly on the metric instead which the visitor receives.  Ignoring this warning will result in deadlock
     *
     * \tparam TDataType the type of data to look for being attached to the metric
     *
     * \param path the path of the metric to query for the custom data
     *
     * \return whether or not the metric has the requested custom data
     */
    template<typename TDataType>
    typename std::enable_if<std::is_base_of<basic_publish_options, TDataType>::value, bool>::type
    has_data_for(const metric_path& path) const;

    /**
     * \brief Get whether or not the custom data exists on the specified metric
     *
     * \tparam TDataType the type of data to look for being attached to the metric
     *
     * \param metric the metric to query for the existence of custom data
     *
     * \return whether or not the metric has the requested data
     */
    template<typename TDataType>
    typename std::enable_if<std::is_base_of<basic_publish_options, TDataType>::value, bool>::type
    has_data_for(const basic_registered_metric& metric) const;

    /**
     * \brief Get whether or not there is a metric at the specified path
     *
     * \warning Figure this out while you're not in a visitor, otherwise, you'll deadlock
     *
     * \return whether or not there is a metric at the specified path
     */
    bool has_metric(const metric_path& path) const;

    /**
     * \brief Get a string representation of the type of metric stored at the specified path
     *
     * \warning Don't use this in a visitor - use the overload that operates directly on the metric instead which the visitor receives.  Ignoring this warning will result in deadlock
     *
     * If there is no metric stored at the path, this will return an empty string
     *
     * \param path the path of the metric to query for the type
     *
     * \return the type of metric at the specified path
     */
    std::string metric_type(const metric_path& path) const;

    /**
     * \brief Get a string representation of the type of metric
     *
     * \param metric the metric to get the type for
     *
     * \return the type of metric at the specified path
     */
    std::string metric_type(const basic_registered_metric& metric) const;

    /**
     * \brief Visit just a single metric in the registry
     *
     * The handler follows the same signature as the visitor on the registry at large or
     * visit_all but it will only be called on the metric requested, if it exists
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
    void visit_all(THandler&& handler) const;
public:
    /**
     * \brief Construct a publisher that will publish from the specified registry
     *
     * \param registry the registry from which the publisher will publish
     */
    constexpr metrics_publisher(metrics_registry<TMetricRepo>& registry) noexcept;
};

}

// ensures there's no undefined values - will pull in impl after the registry is defined if we didn't include it first
#include "metrics_registry.hpp"

#endif //CXXMETRICS_PUBLISHER_HPP
