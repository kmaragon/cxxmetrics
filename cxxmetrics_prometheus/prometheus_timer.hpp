#ifndef CXXMETRICS_PROMETHEUS_TIMER_HPP
#define CXXMETRICS_PROMETHEUS_TIMER_HPP

#include "snapshot_writer.hpp"

namespace cxxmetrics_prometheus
{

template<>
class snapshot_writer<cxxmetrics::timer_snapshot>
{
    void write_header() const
    {
        stream << "# HELP " << internal::name(path) << " " << path.join("/") << " in microseconds\n";
        stream << "# TYPE " << internal::name(path) << " summary\n";
    }

    CXXMETRICS_PROMETHEUS_SNAPSHOT_WRITER_INIT
public:

    void write(const cxxmetrics::tag_collection& tags, const cxxmetrics::timer_snapshot& snapshot)
    {
        const char* comma = "";
        if (tags.begin() != tags.end())
            comma = ",";

        if (options.timer_options().include_count())
            stream << internal::name(path) << "_count{" << internal::tags(tags) << "} " << internal::scale_value(snapshot.count(), options.timer_options()) << "\n";

        stream << internal::name(path) << "_mean{" << internal::tags(tags) << "} " << internal::scale_value(std::chrono::duration_cast<std::chrono::microseconds>(static_cast<std::chrono::nanoseconds>(snapshot.mean())), options.timer_options()) << "\n";
        options.timer_options().quantiles().visit(snapshot, [&](cxxmetrics::quantile q, cxxmetrics::metric_value&& value) {
            stream << internal::name(path) <<
                    '{' << "quantile=\"" << (q.percentile() / 100.0) << "\"" << comma <<
                    internal::tags(tags) << "} " <<
                    internal::scale_value(std::chrono::duration_cast<std::chrono::microseconds>(static_cast<std::chrono::nanoseconds>(value)), options.timer_options()) << "\n";
        });

        if (options.timer_options().include_rates())
        {
            if (options.timer_options().include_mean())
                stream << internal::name(path) << ":rates{" << "window=\"mean\"" << comma << internal::tags(tags) << "} " << internal::scale_value(snapshot.rate().value(), options.timer_options()) << "\n";
            for (const auto& window : snapshot.rate())
                stream << internal::name(path) << ":rates{" << "window=\"" << internal::window(window.first) << "\"" << comma << internal::tags(tags) << "} " << internal::scale_value(cxxmetrics::metric_value(window.second), options.timer_options()) << "\n";
        }
    }
};

}

#endif //CXXMETRICS_PROMETHEUS_HISTOGRAM_HPP
