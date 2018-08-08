#include <catch.hpp>
#include <metrics_registry.hpp>

using namespace cxxmetrics;

TEST_CASE("Registry retreiving same type works", "[metrics_registry]")
{
    metrics_registry<> subject;
    auto* counter = &subject.counter("MyCounter");
    auto* regot = &subject.counter("MyCounter");
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
    auto* counter = &subject.counter("MyCounter");
    auto* regot = &subject.counter("MyCounter", {{"mytag","tagvalue"}});
    REQUIRE(counter != regot);
}

TEST_CASE("Registry retrieving path with wrong type and different tags", "[metrics_registry]")
{
    metrics_registry<> subject;
    subject.counter("MyCounter");
    REQUIRE_THROWS_AS(subject.counter<short>("MyCounter", {{"mytag", "tagvalue"}}), metric_type_mismatch);
}

TEST_CASE("Registry visitor visits counters", "[metrics_registry]")
{
    int names = 0;
    int instances = 0;
    int total = 0;

    metrics_registry<> subject;
    subject.counter("MyCounter") += 10;
    subject.counter("MyCounter", {{"mytag","tagvalue"}}) += 45;

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
