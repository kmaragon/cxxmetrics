#ifndef CXXMETRICS_PROMETHEUS_GAUGE_HPP
#define CXXMETRICS_PROMETHEUS_GAUGE_HPP

#include "snapshot_writer.hpp"

namespace cxxmetrics_prometheus
{

template<>
class snapshot_writer<cxxmetrics::average_value_snapshot>
{
    void write_header() const
    {
        stream << "# TYPE " << internal::name(path) << " gauge\r\n";
    }

    CXXMETRICS_PROMETHEUS_SNAPSHOT_WRITER_INIT
public:

    void write(const cxxmetrics::tag_collection& tags, const cxxmetrics::average_value_snapshot& snapshot)
    {
        // metric_name
        stream << internal::name(path) << '{' << internal::tags(tags) << "} " << internal::scale_value(snapshot.value(), options.value_options()) << "\r\n";
    }
};

}

#endif //CXXMETRICS_PROMETHEUS_GAUGE_HPP
