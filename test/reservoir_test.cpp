#include <catch.hpp>
#include <simple_reservoir.hpp>
#include <uniform_reservoir.hpp>
#include <sliding_window.hpp>
#include "helpers.hpp"

using namespace std;
using namespace cxxmetrics;
using namespace cxxmetrics_literals;

TEST_CASE("Uniform Reservoir on exact count", "[reservoir]")
{
    uniform_reservoir<double, 5> r;

    r.update(10.0);
    r.update(15.0);
    r.update(30.0);
    r.update(40.0);
    r.update(45.0);

    auto s = r.snapshot();

    REQUIRE_THAT(s.min(), Catch::WithinULP(10.0, 1));
    REQUIRE_THAT(s.max(), Catch::WithinULP(45.0, 1));
    REQUIRE(abs(static_cast<double>(s.value<99_p>()) - 45.0) < 1);
    REQUIRE(abs(static_cast<double>(s.value<60_p>()) - 35.0) <= 1);
    REQUIRE_THAT(s.mean(), Catch::WithinULP(28.0, 1));

    uniform_reservoir<double, 5> q = r;
}

TEST_CASE("Uniform Reservoir with overflow", "[reservoir]")
{
    uniform_reservoir<double, 100> r;

    uniform_real_distribution<> d(100.0, 200.0);
    default_random_engine engine;
    for (int i = 0; i < 1000; i++)
        r.update(d(engine));
    auto s = r.snapshot();

    auto min = s.min();
    auto max = s.max();
    auto mean = s.mean();
    auto p50 = s.value<50_p>();

    REQUIRE(abs(static_cast<double>(min) - 100) < 20);
    REQUIRE(abs(static_cast<double>(max) - 200) < 20);
    REQUIRE(abs(static_cast<double>(mean) - 150) < 20);
    REQUIRE(abs(static_cast<double>(p50) - 150) < 20);
}

TEST_CASE("Simple Reservoir overflow", "[reservoir]")
{
    simple_reservoir<double, 5> r;

    r.update(200);
    r.update(10);
    r.update(13);
    r.update(10.0);
    r.update(15.0);
    r.update(30.0);
    r.update(40.0);
    r.update(45.0);

    auto s = r.snapshot();

    REQUIRE_THAT(s.min(), Catch::WithinULP(10.0, 1));
    REQUIRE_THAT(s.max(), Catch::WithinULP(45.0, 1));
    REQUIRE(abs(static_cast<double>(s.value<99_p>()) - 45.0) < 1);
    REQUIRE(abs(static_cast<double>(s.value<60_p>()) - 35.0) <= 1);
    REQUIRE_THAT(s.mean(), Catch::WithinULP(28.0, 1));

    simple_reservoir<double, 5> q = r;
}

TEST_CASE("Sliding Window Reservoir only gets window data", "[reservoir]")
{
    unsigned time = 500;
    mock_clock clk(time);

    sliding_window_reservoir<double, 10, mock_clock> r(100, clk);

    r.update(200);
    time += 20;

    r.update(10);
    time += 20;

    r.update(13);
    time += 20;

    r.update(10.0);
    time += 20;

    r.update(20.0);
    time += 60;

    r.update(30.0);
    r.update(40.0);
    r.update(60.0);
    time += 40;

    auto s = r.snapshot();

    REQUIRE_THAT(s.min(), Catch::WithinULP(20.0, 1));
    REQUIRE_THAT(s.max(), Catch::WithinULP(60.0, 1));
    REQUIRE(abs(static_cast<double>(s.value<99_p>()) - 60.0) < 1);
    REQUIRE(abs(static_cast<double>(s.value<60_p>()) - 40.0) <= 1);
    REQUIRE_THAT(s.mean(), Catch::WithinULP(37.5, 1));

    sliding_window_reservoir<double, 10, mock_clock> q = r;
}
