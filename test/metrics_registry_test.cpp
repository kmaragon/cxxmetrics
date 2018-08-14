#include <catch.hpp>
#include <thread>
#include <cxxmetrics/metrics_registry.hpp>
#include <cxxmetrics/simple_reservoir.hpp>
#include <cxxmetrics/uniform_reservoir.hpp>
#include <cxxmetrics/sliding_window.hpp>

using namespace std::chrono_literals;
using namespace cxxmetrics;
using namespace cxxmetrics_literals;

TEST_CASE("Registry retreiving same type works", "[metrics_registry]")
{
    metrics_registry<> subject;
    auto* counter = subject.counter("MyCounter").get();
    auto* regot = subject.counter("MyCounter").get();
    REQUIRE(counter == regot);
}

TEST_CASE("Registry retrieving path with wrong type throws", "[metrics_registry]")
{
    metrics_registry<> subject;
    subject.counter("MyCounter");
    REQUIRE_THROWS_AS(subject.counter<short>("MyCounter"), metric_type_mismatch);
}

TEST_CASE("Registry retrieving path with different tags", "[metrics_registry]")
{
    metrics_registry<> subject;
    auto* counter = subject.counter("MyCounter").get();
    auto* regot = subject.counter("MyCounter", {{"mytag","tagvalue"}}).get();
    REQUIRE(counter != regot);
}

TEST_CASE("Registry retrieving path with wrong type and different tags", "[metrics_registry]")
{
    metrics_registry<> subject;
    subject.counter("MyCounter");
    REQUIRE_THROWS_AS(subject.counter<short>("MyCounter", {{"mytag", "tagvalue"}}), metric_type_mismatch);
}

TEST_CASE("Registry store existing path with wrong type throws", "[metrics_registry]")
{
    metrics_registry<> subject;
    subject.counter("MyCounter");
    auto ewma = subject.ewma<1_min>("MyEwma");
    REQUIRE_THROWS_AS(subject.register_existing("MyCounter", ewma, {{"mytag", "tagvalue"}}), metric_type_mismatch);
}

TEST_CASE("Registry visitor visits counters", "[metrics_registry]")
{
    int names = 0;
    int instances = 0;
    int total = 0;

    metrics_registry<> subject;
    *subject.counter("MyCounter") += 10;
    *subject.counter("MyCounter", {{"mytag","tagvalue"}}) += 45;

    subject.visit_registered_metrics([&total, &names, &instances](const metric_path& path, basic_registered_metric& metric) {
        ++names;
        metric.visit([&total, &instances](const tag_collection& tags, const value_snapshot& ctr) {
            ++instances;
            total += static_cast<int>(ctr.value());
        });
    });

    REQUIRE(names == 1);
    REQUIRE(instances == 2);
    REQUIRE(total == 55);

    subject.visit_registered_metrics([&total](const metric_path& path, basic_registered_metric& metric) {
        metric.aggregate([&total](const value_snapshot& ctr) {
            total = ctr.value();
        });
    });

    REQUIRE(total == 55);
}

TEST_CASE("Registry supports all the types", "[metrics_registry]")
{
    int names = 0;
    int instances = 0;

    int gaugeProvider = 7;

    metrics_registry<> subject;
    auto myCounter = subject.counter("MyCounter");
    subject.ewma<1_min>("averages"_m/"MyEWMA"/"P2");
    subject.gauge("Gauge"_m/"Other", gaugeProvider);
    subject.histogram("H"_m/"istogramS", simple_reservoir<long, 100>());
    subject.histogram("H"_m/"istogramU", uniform_reservoir<long, 100>());
    subject.histogram("H"_m/"istogramW", sliding_window_reservoir<long, 100>(100s));
    subject.meter<1_sec, 1_min, 1_sec, 5_min>("Meter");
    subject.timer<1_sec, std::chrono::system_clock, simple_reservoir<typename std::chrono::system_clock::duration, 1024>, 1_min, 5_min>("TimerVerbose");
    REQUIRE(subject.register_existing("MyCounter"_m/"Alias", myCounter));

    subject.visit_registered_metrics([&instances, &names](const metric_path& path, basic_registered_metric& metric) {
        ++names;
        metric.visit([&instances](const tag_collection& tags, const auto& ctr) {
            ++instances;
        });
    });

    REQUIRE(names == 9);
    REQUIRE(instances == names);

    names = 0;
    subject.visit_registered_metrics([&names](const metric_path& path, basic_registered_metric& metric) {
        metric.aggregate([&names](const auto& ctr) {
            ++names;
        });
    });

    REQUIRE(names == instances);
}

TEST_CASE("Registry average aggregation", "[metrics_registry]")
{
    metrics_registry<> subject;

    auto& e1 = *subject.ewma<1_min, 50_micro>("emwa1");
    auto& e2 = *subject.ewma<1_min, 50_micro>("emwa1", {{"mytag","tagvalue"}});
    auto& e3 = *subject.ewma<1_min, 50_micro>("emwa1", {{"mytag","tagvalue2"}});

    for (int i = 0; i < 20; i++)
    {
        e1.mark(1000);
        e2.mark(8000);
        e3.mark(18000);
        std::this_thread::sleep_for(50us);
    }

    metric_value total(0);
    subject.visit_registered_metrics([&total](const metric_path& path, basic_registered_metric& metric) {
        metric.aggregate([&total](const value_snapshot& ctr) {
            total = ctr.value();
        });
    });

    REQUIRE(round(total / metric_value(1000.0)) == 9);
}

TEST_CASE("Registry histogram aggregation", "[metrics_registry]")
{
    metrics_registry<> subject;

    auto& h1 = *subject.histogram("histogram", simple_reservoir<int64_t, 100>());
    auto& h2 = *subject.histogram("histogram"_m, simple_reservoir<int64_t, 100>(), {{"mytag","tagvalue"}});
    auto& h3 = *subject.histogram("histogram"_m, simple_reservoir<int64_t, 100>(), {{"mytag","tagvalue2"}});

    for (int i = 1; i <= 100; i++)
    {
        h1.update(i * 10);
        h2.update(i * 42);
        h3.update(i * 97);
    }

    metric_value min(0);
    metric_value max(0);
    metric_value p50(0);
    int64_t count;
    subject.visit_registered_metrics([&](const metric_path& path, basic_registered_metric& metric) {
        metric.aggregate([&](const histogram_snapshot& ctr) {
            min = ctr.min();
            max = ctr.max();
            p50 = ctr.value<50_p>();
            count = ctr.count();
        });
    });

    INFO("Min = " << min << ", max = " << max << ", p50 = " << p50);

    auto h1ss = h1.snapshot();
    auto h2ss = h2.snapshot();
    auto h3ss = h3.snapshot();

    REQUIRE(h1ss.value<50_p>() != p50);
    REQUIRE(h2ss.value<50_p>() != p50);
    REQUIRE(h3ss.value<50_p>() != p50);
    REQUIRE(count == 300);
}

TEST_CASE("Registry meter aggregation", "[metrics_registry]")
{
    metrics_registry<> subject;

    constexpr period interval = 100_micro;

    auto& m1 = *subject.meter<interval, 50_msec, 100_msec, 200_msec>("meter1"_m);
    auto& m2 = *subject.meter<interval, 50_msec, 100_msec, 200_msec>("meter1"_m, {{"mytag","tagvalue"}});
    auto& m3 = *subject.meter<interval, 50_msec, 100_msec, 200_msec>("meter1"_m, {{"mytag","tagvalue2"}});

    metric_value mean(0);
    metric_value m50(0);
    metric_value m100(0);
    metric_value m200(0);

    for (int i = 0; i < 50; i++)
    {
        m1.mark(1000);
        m2.mark(8000);
        m3.mark(18000);
        std::this_thread::sleep_for(interval.to_duration());
    }

    subject.visit_registered_metrics([&](const metric_path& path, basic_registered_metric& metric) {
        metric.aggregate([&](const meter_snapshot& ctr) {
            mean = ctr.value();
            for (const auto& p : ctr)
            {
                if (p.first == 50ms)
                    m50 = (double)p.second;
                else if (p.first == 100ms)
                    m100 = (double)p.second;
                else if (p.first == 200ms)
                    m200 = (double)p.second;
            }
        });
    });

    WARN("Mean = " << mean << ", m50 = " << m50 << ", m100 = " << m100 << ", m200 = " << m200);

    // with the overhead of other stuff happening, the rate won't quite bit 9000
    REQUIRE(round(m50 / metric_value(1000.0)) == 9);
    REQUIRE(round(m100 / metric_value(1000.0)) == 9);
    REQUIRE(round(m200 / metric_value(1000.0)) == 9);
}
