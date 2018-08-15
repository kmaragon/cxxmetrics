#ifndef CXXMETRICS_PROMETHEUS_COUNTER_HPP
#define CXXMETRICS_PROMETHEUS_COUNTER_HPP

#include "snapshot_writer.hpp"

namespace cxxmetrics_prometheus
{

template<>
class snapshot_writer<cxxmetrics::cumulative_value_snapshot>
{
    void write_header() const
    {
        // use untyped instead of counters since counters can be negative per https://prometheus.io/docs/instrumenting/writing_exporters/
        stream << "# TYPE " << internal::name(path) << " untyped\n";
    }

    CXXMETRICS_PROMETHEUS_SNAPSHOT_WRITER_INIT
public:

    void write(const cxxmetrics::tag_collection& tags, const cxxmetrics::cumulative_value_snapshot& snapshot)
    {
        // metric_name
        stream << internal::name(path) << '{' << internal::tags(tags) << "} " << internal::scale_value(snapshot.value(), options.value_options()) << "\n";
    }
};

}

#endif //CXXMETRICS_PROMETHEUS_COUNTER_HPP
