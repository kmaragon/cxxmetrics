#ifndef CXXMETRICS_PROMETHEUS_HISTOGRAM_HPP
#define CXXMETRICS_PROMETHEUS_HISTOGRAM_HPP

#include "snapshot_writer.hpp"

namespace cxxmetrics_prometheus
{

template<>
class snapshot_writer<cxxmetrics::histogram_snapshot>
{
    void write_header() const
    {
        stream << "# TYPE " << internal::name(path) << " summary\r\n";
    }

    CXXMETRICS_PROMETHEUS_SNAPSHOT_WRITER_INIT
public:

    void write(const cxxmetrics::tag_collection& tags, const cxxmetrics::histogram_snapshot& snapshot)
    {
        const char* comma = "";
        if (tags.begin() != tags.end())
            comma = ", ";

        if (options.histogram_options().include_count())
            stream << internal::name(path) << "_count{" << internal::tags(tags) << "} " << internal::scale_value(snapshot.count(), options.histogram_options()) << "\r\n";

        options.histogram_options().quantiles().visit(snapshot, [&](cxxmetrics::quantile q, cxxmetrics::metric_value&& value) {
            stream << internal::name(path) << '{' << "quantile=\"" << (q.percentile() / 100.0) << "\"" << comma << internal::tags(tags) << "} " << internal::scale_value(std::move(value), options.histogram_options()) << "\r\n";
        });
    }
};

}

#endif //CXXMETRICS_PROMETHEUS_HISTOGRAM_HPP
