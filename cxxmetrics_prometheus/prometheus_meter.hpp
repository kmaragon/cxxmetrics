#ifndef CXXMETRICS_PROMETHEUS_METER_HPP
#define CXXMETRICS_PROMETHEUS_METER_HPP

#include "snapshot_writer.hpp"

namespace cxxmetrics_prometheus
{

template<>
class snapshot_writer<cxxmetrics::meter_snapshot>
{
    void write_header() const
    {
        stream << "# TYPE " << internal::name(path) << " gauge\n";
    }

    CXXMETRICS_PROMETHEUS_SNAPSHOT_WRITER_INIT
public:

    void write(const cxxmetrics::tag_collection& tags, const cxxmetrics::meter_snapshot& snapshot)
    {
        const char* comma = "";
        if (tags.begin() != tags.end())
            comma = ",";

        if (options.meter_options().include_mean())
            stream << internal::name(path) << '{' << "window=\"mean\"" << comma << internal::tags(tags) << "} " << internal::scale_value(snapshot.value(), options.meter_options()) << "\n";
        for (const auto& window : snapshot)
            stream << internal::name(path) << '{' << "window=\"" << internal::window(window.first) << "\"" << comma << internal::tags(tags) << "} " << internal::scale_value(cxxmetrics::metric_value(window.second), options.meter_options()) << "\n";
    }
};

}

#endif //CXXMETRICS_PROMETHEUS_METER_HPP
