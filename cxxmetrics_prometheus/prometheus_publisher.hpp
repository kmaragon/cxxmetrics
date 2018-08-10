#ifndef CXXMETRICS_PROMETHEUS_PUBLISHER_HPP
#define CXXMETRICS_PROMETHEUS_PUBLISHER_HPP

#include <cxxmetrics/publisher.hpp>
#include "prometheus_counter.hpp"
#include "prometheus_gauge.hpp"
#include "prometheus_meter.hpp"
#include "prometheus_histogram.hpp"
#include "prometheus_timer.hpp"

namespace cxxmetrics_prometheus
{

template<typename TMetricRepo>
class prometheus_publisher : public cxxmetrics::metrics_publisher<TMetricRepo>
{
public:
    prometheus_publisher(cxxmetrics::metrics_registry<TMetricRepo>& registry) :
            cxxmetrics::metrics_publisher<TMetricRepo>(registry)
    { }

    void write(std::ostream& into)
    {
        this->visit_all([this, &into](const cxxmetrics::metric_path& name, cxxmetrics::basic_registered_metric& metric) {
            const auto& options = this->effective_options(metric);
            bool header = false;

            if (name.begin() == name.end())
                return;

            metric.visit([&](const cxxmetrics::tag_collection& tags, const auto& snapshot) {
                using snapshot_type = typename std::decay<decltype(snapshot)>::type;
                snapshot_writer<snapshot_type> writer(into, name, header, options);
                writer.write(tags, snapshot);
            });
        });
    }
};

}

#undef CXXMETRICS_PROMETHEUS_SNAPSHOT_WRITER_INIT

#endif //CXXMETRICS_PROMETHEUS_PUBLISHER_HPP
