#include <catch2/catch_all.hpp>
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
    REQUIRE_THAT(out, Catch::Matchers::StartsWith( "# TYPE ") &&
            Catch::Matchers::ContainsSubstring(" untyped") &&
            Catch::Matchers::ContainsSubstring("1MyCounter") &&
            Catch::Matchers::ContainsSubstring("1200100") &&
            Catch::Matchers::ContainsSubstring(":COUNTER") &&
            Catch::Matchers::ContainsSubstring("tag_value\\\"with quotes\\\"") &&
            Catch::Matchers::ContainsSubstring("x2=\"123523\"") &&
            Catch::Matchers::ContainsSubstring("tag_name2__"));
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
    REQUIRE_THAT(out, Catch::Matchers::StartsWith( "# TYPE MyGauge:value gauge") &&
            Catch::Matchers::ContainsSubstring("MyGauge:value") &&
            Catch::Matchers::ContainsSubstring("tag_name2=\"tag_value\"") &&
            Catch::Matchers::ContainsSubstring("923.005") &&
            Catch::Matchers::ContainsSubstring("x2=\"123523\""));
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
    REQUIRE_THAT(out, Catch::Matchers::StartsWith( "# TYPE MyMeter gauge") &&
            Catch::Matchers::ContainsSubstring("MyMeter") &&
            Catch::Matchers::ContainsSubstring("tag_name2=\"tag_value\"") &&
            Catch::Matchers::ContainsSubstring("mean") &&
            Catch::Matchers::ContainsSubstring("1sec") &&
            Catch::Matchers::ContainsSubstring("1min") &&
            Catch::Matchers::ContainsSubstring("5min") &&
            Catch::Matchers::ContainsSubstring("x2=\"123523\""));
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
    REQUIRE_THAT(out, Catch::Matchers::ContainsSubstring( "# TYPE MyHistogram summary") &&
            Catch::Matchers::ContainsSubstring("MyHistogram_count") &&
            Catch::Matchers::ContainsSubstring("mytag=\"tagvalue2\"") &&
            Catch::Matchers::ContainsSubstring(".5") &&
            Catch::Matchers::ContainsSubstring(".9") &&
            Catch::Matchers::ContainsSubstring(".99"));
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
    REQUIRE_THAT(out, Catch::Matchers::ContainsSubstring( "# TYPE MyTimer summary") &&
            Catch::Matchers::ContainsSubstring("MyTimer_count") &&
            Catch::Matchers::ContainsSubstring(".5") &&
            Catch::Matchers::ContainsSubstring(".9") &&
            Catch::Matchers::ContainsSubstring(".99") &&
            Catch::Matchers::ContainsSubstring("tag_name2=\"tag_value\"") &&
            Catch::Matchers::ContainsSubstring("mean") &&
            Catch::Matchers::ContainsSubstring("10sec") &&
            Catch::Matchers::ContainsSubstring("1usec") &&
            Catch::Matchers::ContainsSubstring("1min") &&
            Catch::Matchers::ContainsSubstring("5min") &&
            Catch::Matchers::ContainsSubstring("x2=\"123523\""));
}
