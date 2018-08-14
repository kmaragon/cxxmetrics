#ifndef CXXMETRICS_SNAPSHOT_WRITER_HPP
#define CXXMETRICS_SNAPSHOT_WRITER_HPP

#include <ostream>
#include <cxxmetrics/snapshots.hpp>
#include <cxxmetrics/publisher.hpp>

namespace cxxmetrics_prometheus
{

namespace internal
{

inline cxxmetrics::metric_value scale_value(cxxmetrics::metric_value&& value, const cxxmetrics::value_publish_options& opts)
{
    if (opts.scale())
        return value * cxxmetrics::metric_value(opts.scale().factor());
    return std::move(value);
}

inline std::ostream& format_name_element(std::ostream&into, const std::string& element)
{
    for (std::size_t i = 0; i < element.size(); i++)
    {
        char c = element[i];
        if (!isalnum(c))
            into << '_';
        else
            into << c;
    }

    return into;
}

inline std::ostream& format_name(std::ostream& into, const cxxmetrics::metric_path& path)
{
    auto elem = path.begin();
    if (std::isdigit((*elem)[0]))
        into << '_';

    format_name_element(into, *elem);
    for (++elem; elem != path.end(); ++elem)
    {
        into << ':';
        format_name_element(into, *elem);
    }

    return into;
}

inline std::ostream& format_tag_value(std::ostream& into, const std::string& value)
{
    for (auto c : value)
    {
        if (c == '"')
            into << "\\\"";
        else
            into << c;
    }

    return into;
}

inline std::ostream& format_tags(std::ostream& into, const cxxmetrics::tag_collection& tags)
{
    auto tag = tags.begin();
    if (tag == tags.end())
        return into;

    format_name(into, tag->first);
    into << "=\"";
    format_tag_value(into, tag->second) << "\"";
    for (++tag; tag != tags.end(); ++tag)
    {
        into << ", ";
        format_name(into, tag->first) << "=\"";
        format_tag_value(into, tag->second) << "\"";
    }

    return into;
}

template<typename TRep, typename TPer>
inline std::ostream& format_window(std::ostream& into, const std::chrono::duration<TRep, TPer>& time)
{
    using namespace std::chrono_literals;
    if (time >= 1h)
    {
        into << std::chrono::duration_cast<std::chrono::hours>(time).count() << "hr";
        return into;
    }
    if (time >= 1min)
    {
        into << std::chrono::duration_cast<std::chrono::minutes>(time).count() << "min";
        return into;
    }
    if (time >= 1s)
    {
        into << std::chrono::duration_cast<std::chrono::seconds>(time).count() << "sec";
        return into;
    }
    if (time >= 1ms)
    {
        into << std::chrono::duration_cast<std::chrono::milliseconds>(time).count() << "msec";
        return into;
    }
    if (time >= 1us)
    {
        into << std::chrono::duration_cast<std::chrono::microseconds>(time).count() << "usec";
        return into;
    }

    into << std::chrono::duration_cast<std::chrono::nanoseconds>(time).count() << "nsec";
    return into;
}

template<typename TRep, typename TPer>
struct prometheus_time_window_t
{
    const std::chrono::duration<TRep, TPer>& duration;
    prometheus_time_window_t(const std::chrono::duration<TRep, TPer>& d) : duration(d) { }
};

template<typename TRep, typename TPer>
inline prometheus_time_window_t<TRep, TPer> window(const std::chrono::duration<TRep, TPer>& d)
{
    return prometheus_time_window_t<TRep, TPer>(d);
}

template<typename TRep, typename TPer>
inline std::ostream& operator<<(std::ostream& stream, prometheus_time_window_t<TRep, TPer> w)
{
    return format_window(stream, w.duration);
}

struct prometheus_tags_t
{
    const cxxmetrics::tag_collection& tags;
    prometheus_tags_t(const cxxmetrics::tag_collection& t) : tags(t) { }
};

inline prometheus_tags_t tags(const cxxmetrics::tag_collection& tags)
{
    return prometheus_tags_t(tags);
}

inline std::ostream& operator<<(std::ostream& stream, prometheus_tags_t t)
{
    return format_tags(stream, t.tags);
}

struct prometheus_name_t
{
    const cxxmetrics::metric_path& name;
    prometheus_name_t(const cxxmetrics::metric_path& n) : name(n) { }
};

prometheus_name_t name(const cxxmetrics::metric_path& path)
{
    return prometheus_name_t(path);
}

inline std::ostream& operator<<(std::ostream& stream, prometheus_name_t t)
{
    return format_name(stream, t.name);
}

};

#define CXXMETRICS_PROMETHEUS_SNAPSHOT_WRITER_INIT \
private: \
    std::ostream& stream; \
    const cxxmetrics::metric_path& path; \
    const cxxmetrics::publish_options& options; \
public: \
    snapshot_writer(std::ostream& out, const cxxmetrics::metric_path& name, bool& header_written, const cxxmetrics::publish_options& opts) : \
            stream(out), \
            path(name), \
            options(opts) \
    { \
       if (!header_written) \
       { \
           write_header(); \
           header_written = true; \
       } \
    } \
private:

template<typename TSnapshot>
class snapshot_writer
{
};

}

#endif //CXXMETRICS_SNAPSHOT_WRITER_HPP
