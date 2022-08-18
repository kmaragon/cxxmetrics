#include <catch2/catch.hpp>
#include <thread>
#include <cxxmetrics/metrics_registry.hpp>
#include <cxxmetrics/simple_reservoir.hpp>
#include <cxxmetrics/sliding_window.hpp>

using namespace cxxmetrics;
using namespace cxxmetrics_literals;
using namespace std::chrono_literals;

template<typename TRepo = default_repository>
class test_publisher : public metrics_publisher<TRepo>
{
public:
    constexpr test_publisher(metrics_registry<TRepo>& registry) noexcept :
            metrics_publisher<TRepo>(registry)
    { }

    template<typename T, typename... TArgs>
    T& data(TArgs&&... args)
    {
        return this->template get_data<T>(std::forward<TArgs>(args)...);
    }

    template<typename T, typename... TArgs>
    T* metric_data(const metric_path& name, TArgs&&... args)
    {
        return this->template get_data_for<T>(name, std::forward<TArgs>(args)...);
    }

    template<typename T>
    bool has_metric_data(const metric_path& name)
    {
        return this->template has_data_for<T>(name);
    }

    bool exists(const metric_path& name) const
    {
        return this->has_metric(name);
    }

    std::string type_of(const metric_path& name) const
    {
        return this->metric_type(name);
    }

    std::string type_of(const basic_registered_metric& metric) const
    {
        return this->metric_type(metric);
    }

    template<typename THandler>
    void visit_metric(const metric_path& path, THandler&& handler) const
    {
        return this->visit_one(path, std::forward<THandler>(handler));
    }

    template<typename THandler>
    void visit_registry(THandler&& handler) const
    {
        return this->visit_all(std::forward<THandler>(handler));
    }

    const auto& opts(basic_registered_metric& metric)
    {
        return this->effective_options(metric);
    }
};

struct test_pubdata : public basic_publish_options
{
    int value_;
public:
    test_pubdata() :
            value_(0)
    { }

    test_pubdata(int v) :
            value_(v)
    { }

    int value() const
    {
        return value_;
    }
};

TEST_CASE("Publisher can get custom publish data", "[publisher]")
{
    metrics_registry<> r;
    test_publisher<> subject(r);

    auto& v = subject.data<test_pubdata>(10);
    REQUIRE(v.value() == 10);

    auto& e = subject.data<test_pubdata>(2000);
    REQUIRE(e.value() == 10);

    REQUIRE(&e == &v);
}

TEST_CASE("Publisher can get metric-attached custom publish data", "[publisher]")
{
    metrics_registry<> r;
    test_publisher<> subject(r);

    auto path = "Test"_m/"Counter"/"blah";

    REQUIRE(!subject.exists(path));
    r.counter(path);
    REQUIRE(subject.exists(path));

    REQUIRE(!subject.has_metric_data<test_pubdata>(path));
    auto v = subject.metric_data<test_pubdata>(path, 10);
    REQUIRE(v != nullptr);
    REQUIRE(subject.has_metric_data<test_pubdata>(path));
    REQUIRE(v->value() == 10);

    auto e = subject.metric_data<test_pubdata>(path, 2000);
    REQUIRE(e->value() == 10);
    REQUIRE(e == v);

    REQUIRE(subject.metric_data<test_pubdata>(path/"other") == nullptr);
    REQUIRE(!subject.has_metric_data<test_pubdata>(path/"other"));
}

TEST_CASE("Publisher metric types are correctly resolved", "[publisher]")
{
    int gaugeProvider = 7;
    int gp2 = 8;

    metrics_registry<> r;
    test_publisher<> subject(r);
    auto myCounter = r.counter("MyCounter");
    REQUIRE(subject.type_of("MyCounter") == "counter");

    r.ewma<1_min>("MyEWMA");
    REQUIRE(subject.type_of("MyEWMA") == "ewma");

    r.gauge("Gauge"/"Ref"_m, gaugeProvider);
    r.gauge("Gauge"/"Ptr"_m, &gp2);
    REQUIRE(subject.type_of("Gauge"/"Ref"_m) == "gauge");
    REQUIRE(subject.type_of("Gauge"/"Ptr"_m) == "gauge");

    r.histogram("HistogramS", simple_reservoir<long, 100>());
    r.histogram("HistogramU", uniform_reservoir<long, 100>());
    r.histogram("HistogramW", sliding_window_reservoir<long, 100>(100s));
    REQUIRE(subject.type_of("HistogramS") == "histogram");
    REQUIRE(subject.type_of("HistogramU") == "histogram");
    REQUIRE(subject.type_of("HistogramW") == "histogram");

    r.meter<1_sec, 1_min, 1_sec, 5_min>("Meter");
    REQUIRE(subject.type_of("Meter") == "meter");

    r.timer<1_sec, std::chrono::system_clock, simple_reservoir<typename std::chrono::system_clock::duration, 1024>, 1_min, 5_min>("TimerVerbose");
    REQUIRE(subject.type_of("TimerVerbose") == "timer");

    r.register_existing("MyCounter"/"Alias"_m, myCounter);
    REQUIRE(subject.type_of("MyCounter"/"Alias"_m) == "counter");

    REQUIRE(subject.type_of("NonExistent"/"Metric"_m).empty());
}

TEST_CASE("Publisher visiting one metric only visits one metric", "[publisher]")
{
    metrics_registry<> r;
    test_publisher<> subject(r);
    *r.counter("MyCounter") += 99;
    r.ewma<1_min>("MyEWMA");
    r.meter<1_sec, 1_min, 1_sec, 5_min>("Meter");

    int count = 0;
    metric_value value(0);
    auto visitor = [&count, &value](const metric_path& path, basic_registered_metric& metric) {
        ++count;
        metric.aggregate([&value](const value_snapshot& ss) { value = ss.value(); });
    };

    subject.visit_metric("MyCounter", visitor);
    REQUIRE(count == 1);
    REQUIRE(value == metric_value(99));

    subject.visit_metric("Nonexistent"/"Metric"_m, visitor);
    REQUIRE(count == 1);
}

TEST_CASE("Publisher visit_all sanity check", "[publisher]")
{
    metrics_registry<> r;
    test_publisher<> subject(r);
    r.counter("MyCounter");
    r.ewma<1_min>("MyEWMA");
    r.meter<1_sec, 1_min, 1_sec, 5_min>("Meter");

    int count = 0;
    auto visitor = [&count](const metric_path& path, basic_registered_metric& metric) {
        ++count;
    };

    subject.visit_registry(visitor);
    REQUIRE(count == 3);
}

TEST_CASE("Publisher can get value publish options", "[publisher]")
{
    metrics_registry<> r;
    test_publisher<> subject(r);
    *r.counter("MyCounter") += 100;
    r.ewma<1_min>("MyEWMA");
    r.meter<1_sec, 1_min, 1_sec, 5_min>("Meter");

    int count = 0;
    metric_value value(0);
    auto visitor = [&](const metric_path& path, basic_registered_metric& metric) {
        ++count;
        metric.aggregate([&](const value_snapshot& ss) {
            if (subject.type_of(metric) != "counter")
                return;

            auto& opts = subject.opts(metric);
            if (opts.value_options().scale())
                value = ss.value() * metric_value(opts.value_options().scale().factor());
            else
                value = ss.value();
        });
    };

    subject.visit_registry(visitor);
    REQUIRE(value == metric_value(100));
    REQUIRE(count == 3);

    r.publish_options(publish_options(value_publish_options(0.5)));
    subject.visit_registry(visitor);
    REQUIRE(value == metric_value(50));
    REQUIRE(count == 6);
}

TEST_CASE("Publisher can handle metric overrides on publish options", "[publisher]")
{
    metrics_registry<> r;
    test_publisher<> subject(r);
    auto ctr = r.counter("MyCounter1");
    *ctr += 1000;
    r.register_existing("MyCounter2", ctr);

    int count = 0;
    metric_value value(0);
    auto visitor = [&](const metric_path& path, basic_registered_metric& metric) {
        ++count;
        metric.aggregate([&](const value_snapshot& ss) {
            if (subject.type_of(metric) != "counter")
                return;

            auto& opts = subject.opts(metric);
            if (opts.value_options().scale())
                value += (ss.value() * metric_value(opts.value_options().scale().factor()));
            else
                value += ss.value();
        });
    };

    r.publish_options("MyCounter2", publish_options(value_publish_options(0.5)));

    subject.visit_registry(visitor);
    REQUIRE(value == metric_value(1500));
    REQUIRE(count == 2);
}

TEST_CASE("Publisher can get meter publish options", "[publisher]")
{
    metrics_registry<> r;
    test_publisher<> subject(r);
    *r.counter("MyCounter") += 100;
    auto& m = *r.meter<1_sec, 1_min, 1_sec, 5_min>("Meter");

    for (int i = 0; i < 50; i++)
        m.mark(10000);

    int count = 0;
    metric_value value(0);
    auto visitor = [&](const metric_path& path, basic_registered_metric& metric) {
        ++count;
        metric.aggregate([&](const meter_snapshot& ss) {
            auto& opts = subject.opts(metric);
            if (opts.meter_options().include_mean())
                value = ss.value();
            else
                value = 0;
        });
    };

    subject.visit_registry(visitor);
    REQUIRE(round(value) >= 1);
    REQUIRE(count == 2);

    r.publish_options(publish_options(meter_publish_options(false)));
    subject.visit_registry(visitor);
    REQUIRE(value == metric_value(0));
    REQUIRE(count == 4);
}

TEST_CASE("Publisher can get histogram options", "[publisher]")
{
    metrics_registry<> r;
    test_publisher<> subject(r);
    *r.counter("MyCounter") += 100;
    auto& h1 = *r.histogram("histogram", simple_reservoir<int64_t, 100>());

    for (int i = 0; i < 50; i++)
        h1.update(i * 2000);

    int count = 0;
    metric_value value(0);
    std::unordered_map<long double, metric_value> values;
    auto visitor = [&](const metric_path& path, basic_registered_metric& metric) {
        ++count;
        metric.aggregate([&](const histogram_snapshot& ss) {
            auto& opts = subject.opts(metric);
            if (opts.histogram_options().include_count())
                value = ss.count();
            else
                value = 0;

            auto qvisitors = opts.histogram_options().quantiles();
            qvisitors->visit(ss, [&](quantile q, const metric_value& v)
            {
                values.emplace(q, round(v));
            });

            ss.sample(9, [&](const metric_value& v, quantile q) {
                values.emplace(q, round(v));
            });
        });
    };

    auto print_percentiles = [&](const auto& dict) {
        INFO("Percentiles: ");
        for (const auto &pair : dict)
            INFO("    " << pair.first << ": " << pair.second);
    };

    subject.visit_registry(visitor);
    REQUIRE(value == metric_value(50));
    REQUIRE(values.size() == 12); // defaults
    REQUIRE(count == 2);

    print_percentiles(values);
    values.clear();

    r.publish_options(publish_options(histogram_publish_options(quantile_options<99_p>(), false)));
    subject.visit_registry(visitor);

    print_percentiles(values);
    REQUIRE(value == metric_value(0));
    REQUIRE(values.size() == 10);
    REQUIRE(count == 4);
}
