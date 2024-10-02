#include <catch2/catch_all.hpp>
#include <cxxmetrics/counter.hpp>

using namespace cxxmetrics;

TEST_CASE("Counter incr and wrappers work", "[counter]")
{
    counter<int64_t> a(15);
    a += 5;
    REQUIRE(a == 20);

    ++a;
    REQUIRE(a == 21);

    a -= 16;
    REQUIRE(a == 5);

    --a;
    REQUIRE(a == 4);

    a = 10;
    REQUIRE(a == 10);

    REQUIRE(a.metric_type().find("counter") != std::string::npos);
}

TEST_CASE("Counter excercise snapshot", "[counter]")
{
    counter<double> a(15.7);
    REQUIRE(a.snapshot() == 15.7);
}
