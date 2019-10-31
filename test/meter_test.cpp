#include <catch2/catch.hpp>
#include <thread>
#include <cxxmetrics/meter.hpp>
#include <ctti/type_id.hpp>
#include "helpers.hpp"

using namespace std;
using namespace std::chrono_literals;
using namespace cxxmetrics;
using namespace cxxmetrics_literals;

TEST_CASE("Meter parameters with mean get sorted for equal types", "[meter]")
{
    meter<5_msec, 1_min, 30_min, 48_hour, 24_hour, 1_hour> m;
    meter<5_msec, 1_hour, 30_min, 24_hour, 48_hour, 48_hour, 30_min, 1_min> n;

    REQUIRE(m.metric_type() == ((internal::metric *)&n)->metric_type());
    INFO("metric_type: " << m.metric_type());
}

TEST_CASE("Meter copy assignment works", "[meter]")
{
    meter<5_msec, 1_min, 30_min, 48_hour, 24_hour, 1_hour> m;
    meter<5_msec, 1_min, 30_min, 48_hour, 24_hour, 1_hour> n(m);

    m = n;
}

TEST_CASE("Meter initializes correctly", "[meter]")
{
    meter<5_msec, 1_min, 1_sec> m;
    REQUIRE(m.rate<1_min>().rate == 0);
    REQUIRE(m.rate<1_sec>().rate == 0);
    REQUIRE(m.mean() == 0);

    auto ss = m.snapshot();
    REQUIRE(ss.value() == metric_value(0));
}

TEST_CASE("Meter rates are passed on", "[meter]")
{
    unsigned clock = 1;
    mock_clock clk(clock);

    internal::_meter_impl<mock_clock, 1, 1, 8, 20, 50> m(clk);

    for(int i = 0; i < 10; i++)
    {
        m.mark(10);
        clock++;
    }

    REQUIRE(round(m.get_rate<1>()) == 10);
    REQUIRE(round(m.get_rate<8>()) == 10);
    REQUIRE(round(m.get_rate<20>()) == 10);
    REQUIRE(round(m.get_rate<50>()) == 10);
    REQUIRE(round(m.mean()) == 10);

    clock += 100;
    m.mark(1000);

    clock += 1;

    REQUIRE(m.get_rate<1>()> m.get_rate<8>());
    REQUIRE(m.get_rate<8>()> m.get_rate<20>());
    REQUIRE(m.get_rate<20>()> m.get_rate<50>());
    REQUIRE_THAT(m.mean(), Catch::WithinULP(1100.0 / 111.0, 1));
}

TEST_CASE("Meter snapshot", "[meter]")
{
    meter<5_micro, 1_min, 30_min, 48_hour, 24_hour, 1_hour> m;
    m.mark(100);
    std::this_thread::sleep_for(10us);

    auto ss = m.snapshot();
    for (const auto& pair : ss) {
        REQUIRE(pair.second != metric_value(0.0));
    }

    REQUIRE(ss.value() != metric_value(0.0));
}

