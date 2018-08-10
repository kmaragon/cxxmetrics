#ifndef CXXMETRICS_PUBLISHER_IMPL_HPP
#define CXXMETRICS_PUBLISHER_IMPL_HPP

#include "publisher.hpp"
#include "metrics_registry.hpp"

namespace cxxmetrics
{

template<typename TMetricRepo>
constexpr metrics_publisher<TMetricRepo>::metrics_publisher(metrics_registry<TMetricRepo> &registry) noexcept :
        registry_(registry)
{}

template<typename TMetricRepo>
const publish_options& metrics_publisher<TMetricRepo>::effective_options(basic_registered_metric& metric) const
{
    if (!has_data_for<publish_options>(metric))
        return registry_.publish_options();

    // In the absence of data being removed in other threads, (removing data isn't supported) this is safe
    return const_cast<metrics_publisher<TMetricRepo>*>(this)->get_data_for<publish_options>(metric);
}

template<typename TMetricRepo>
template<typename TDataType, typename... TBuildArgs>
typename std::enable_if<std::is_base_of<basic_publish_options, TDataType>::value, TDataType>::type&
metrics_publisher<TMetricRepo>::get_data(TBuildArgs&&... args)
{
    return registry_.template get_publish_data<TDataType>(std::forward<TBuildArgs>(args)...);
}

template<typename TMetricRepo>
template<typename TDataType, typename... TBuildArgs>
typename std::enable_if<std::is_base_of<basic_publish_options, TDataType>::value, TDataType>::type*
metrics_publisher<TMetricRepo>::get_data_for(const metric_path& path, TBuildArgs&&... args)
{
    auto res = registry_.try_get(path);
    if (res == nullptr)
        return nullptr;

    return &get_data_for<TDataType>(*res, std::forward<TBuildArgs>(args)...);
}

template<typename TMetricRepo>
template<typename TDataType, typename... TBuildArgs>
typename std::enable_if<std::is_base_of<basic_publish_options, TDataType>::value, TDataType>::type&
metrics_publisher<TMetricRepo>::get_data_for(basic_registered_metric& metric, TBuildArgs&&... args)
{
    return metric.template get_or_create_publish_data<TDataType>(std::forward<TBuildArgs>(args)...);
}

template<typename TMetricRepo>
template<typename TDataType>
typename std::enable_if<std::is_base_of<basic_publish_options, TDataType>::value, bool>::type
metrics_publisher<TMetricRepo>::has_data_for(const metric_path& path) const
{
    auto res = registry_.try_get(path);
    if (res == nullptr)
        return false;

    return has_data_for<TDataType>(*res);
}

template<typename TMetricRepo>
template<typename TDataType>
typename std::enable_if<std::is_base_of<basic_publish_options, TDataType>::value, bool>::type
metrics_publisher<TMetricRepo>::has_data_for(const basic_registered_metric& metric) const
{
    return metric.template try_get_publish_data<TDataType>() != nullptr;
}

template<typename TMetricRepo>
bool metrics_publisher<TMetricRepo>::has_metric(const metric_path& path) const
{
    return registry_.try_get(path) != nullptr;
}

template<typename TMetricRepo>
std::string metrics_publisher<TMetricRepo>::metric_type(const basic_registered_metric& metric) const
{
    auto result = metric.type();

    // try to parse this string
    int templdepth = 0;
    std::size_t start = 0;
    std::size_t end = 0;

    bool hadcolon = false;
    for (std::size_t i = 0; i < result.length(); i++)
    {
        char c = result[i];
        if (c == ':')
        {
            if (templdepth)
                continue;

            if (hadcolon)
            {
                start = i+1;
                end = 0;
            }
            else
                hadcolon = true;

            continue;
        }

        hadcolon = false;
        switch (c)
        {
            case '<':
                if (++templdepth == 1)
                    end = i;
                continue;
            case '>':
                --templdepth;
                continue;
        }
    }

    if (end == 0)
        return result.substr(start);
    if (end <= start)
        return {};

    return result.substr(start, end - start);
}

template<typename TMetricRepo>
std::string metrics_publisher<TMetricRepo>::metric_type(const metric_path& path) const
{
    auto res = registry_.try_get(path);
    if (res == nullptr)
        return {};

    return metric_type(*res);
}

template<typename TMetricRepo>
template<typename THandler>
void metrics_publisher<TMetricRepo>::visit_one(const metric_path& path, THandler&& handler) const
{
    auto res = registry_.try_get(path);
    if (res == nullptr)
        return;

    handler(path, *res);
}

template<typename TMetricRepo>
template<typename THandler>
void metrics_publisher<TMetricRepo>::visit_all(THandler&& handler) const
{
    registry_.visit_registered_metrics(std::forward<THandler>(handler));
}

}

#endif //CXXMETRICS_PUBLISHER_IMPL_HPP
