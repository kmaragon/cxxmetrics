#include <gtest/gtest.h>
#include <metrics_registry.hpp>

using namespace cxxmetrics;

TEST(metrics_registry_test, test_retreive_same_type)
{
    metrics_registry<> subject;
    auto* counter = &subject.counter("MyCounter");
    auto* regot = &subject.counter("MyCounter");
    ASSERT_EQ(counter, regot);
}

TEST(metrics_registry_test, test_retreive_wrong_type)
{
    metrics_registry<> subject;
    subject.counter("MyCounter");
    ASSERT_THROW(subject.counter<short>("MyCounter"), metric_type_mismatch);
}

TEST(metrics_registry_test, test_retreive_same_type_different_tags)
{
    metrics_registry<> subject;
    auto* counter = &subject.counter("MyCounter");
    auto* regot = &subject.counter("MyCounter", {{"mytag","tagvalue"}});
    ASSERT_NE(counter, regot);
}

TEST(metrics_registry_test, test_retreive_wrong_type_different_tags)
{
    metrics_registry<> subject;
    subject.counter("MyCounter");
    ASSERT_THROW(subject.counter<short>("MyCounter", {{"mytag", "tagvalue"}}), metric_type_mismatch);
}
