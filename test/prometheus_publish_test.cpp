#include <catch2/catch.hpp>
#include <sstream>
#include <cxxmetrics_prometheus/prometheus_publisher.hpp>
#include <cxxmetrics/simple_reservoir.hpp>

using namespace cxxmetrics;
using namespace cxxmetrics_literals;
using namespace cxxmetrics_prometheus;

TEST_CASE("Prometheus Publisher can publish counter values", "[prometheus]")
{
    metrics_registry<> r;
    prometheus_publisher<decltype(r)::repository_type> subject(r);
    *r.counter("1MyCounter%#@#(@#:][\\"/"COUNTER"_m, {{"tag_name2^$", "tag_value\"with quotes\""}, {"x2", 123523}}) += 1200100;

    std::stringstream stream;
    subject.write(stream);

    auto out = stream.str();
    WARN(out);
    REQUIRE_THAT(out, Catch::StartsWith( "# TYPE ") &&
            Catch::Contains(" untyped") &&
            Catch::Contains("1MyCounter") &&
            Catch::Contains("1200100") &&
            Catch::Contains(":COUNTER") &&
            Catch::Contains("tag_value\\\"with quotes\\\"") &&
            Catch::Contains("x2=\"123523\"") &&
            Catch::Contains("tag_name2__"));
}

TEST_CASE("Prometheus Publisher can publish gauge values", "[prometheus]")
{
    double metric_value = 923.005;
    metrics_registry<> r;
    prometheus_publisher<decltype(r)::repository_type> subject(r);
    r.gauge("MyGauge"/"value"_m, metric_value, {{"tag_name2", "tag_value"}, {"x2", 123523}});

    std::stringstream stream;
    subject.write(stream);

    auto out = stream.str();
    WARN(out);
    REQUIRE_THAT(out, Catch::StartsWith( "# TYPE MyGauge:value gauge") &&
            Catch::Contains("MyGauge:value") &&
            Catch::Contains("tag_name2=\"tag_value\"") &&
            Catch::Contains("923.005") &&
            Catch::Contains("x2=\"123523\""));
}

TEST_CASE("Prometheus Publisher can publish meter values", "[prometheus]")
{
    metrics_registry<> r;
    prometheus_publisher<decltype(r)::repository_type> subject(r);
    auto& m = *r.meter<1_sec, 1_min, 1_sec, 5_min>("MyMeter", {{"tag_name2", "tag_value"}, {"x2", 123523}});
    for (int i = 0; i < 10; i++)
        m.mark(10000);

    std::stringstream stream;
    subject.write(stream);

    auto out = stream.str();
    WARN(out);
    REQUIRE_THAT(out, Catch::StartsWith( "# TYPE MyMeter gauge") &&
            Catch::Contains("MyMeter") &&
            Catch::Contains("tag_name2=\"tag_value\"") &&
            Catch::Contains("mean") &&
            Catch::Contains("1sec") &&
            Catch::Contains("1min") &&
            Catch::Contains("5min") &&
            Catch::Contains("x2=\"123523\""));
}

TEST_CASE("Prometheus Publisher can publish histogram values", "[prometheus]")
{
    metrics_registry<> r;
    prometheus_publisher<decltype(r)::repository_type> subject(r);
    auto& hist = *r.histogram("MyHistogram"_m, cxxmetrics::simple_reservoir<int64_t, 100>(), {{"mytag","tagvalue2"}});

    for (int i = 1; i <= 100; i++)
        hist.update(i * 97);

    std::stringstream stream;
    subject.write(stream);

    auto out = stream.str();
    WARN(out);
    REQUIRE_THAT(out, Catch::Contains( "# TYPE MyHistogram summary") &&
            Catch::Contains("MyHistogram_count") &&
            Catch::Contains("mytag=\"tagvalue2\"") &&
            Catch::Contains(".5") &&
            Catch::Contains(".9") &&
            Catch::Contains(".99"));
}

TEST_CASE("Prometheus Publisher can publish timer values", "[prometheus]")
{
    using reservoir_type = simple_reservoir<std::chrono::system_clock::duration, 4>;
    metrics_registry<> r;
    prometheus_publisher<decltype(r)::repository_type> subject(r);
    auto& t = *r.timer<100_micro, std::chrono::system_clock, reservoir_type, true, 5_min, 1_min, 10_sec>("MyTimer", reservoir_type(), {{"tag_name2", "tag_value"}, {"x2", 123523}});
    t.update(std::chrono::microseconds(1000));
    t.update(std::chrono::microseconds(10));
    t.update(std::chrono::microseconds(20));
    t.update(std::chrono::microseconds(40));
    t.update(std::chrono::microseconds(80));

    std::stringstream stream;
    subject.write(stream);

    auto out = stream.str();
    WARN(out);
    REQUIRE_THAT(out, Catch::Contains( "# TYPE MyTimer summary") &&
            Catch::Contains("MyTimer_count") &&
            Catch::Contains(".5") &&
            Catch::Contains(".9") &&
            Catch::Contains(".99") &&
            Catch::Contains("tag_name2=\"tag_value\"") &&
            Catch::Contains("mean") &&
            Catch::Contains("10sec") &&
            Catch::Contains("1usec") &&
            Catch::Contains("1min") &&
            Catch::Contains("5min") &&
            Catch::Contains("x2=\"123523\""));
}
