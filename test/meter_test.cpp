#include <catch.hpp>
#include <meter.hpp>
#include <thread>
#include <ctti/type_id.hpp>
#include "helpers.hpp"

using namespace std;
using namespace std::chrono_literals;
using namespace cxxmetrics;
using namespace cxxmetrics::literals;

TEST_CASE("Meter parameters with mean get sorted for equal types", "[meter]")
{
    meter<1_min, 30_min, 7_day, 1_day, 1_hour> m(5ms);
    meter<1_hour, 30_min, 1_day, 7_day, 7_day, 30_min, 1_min> n(5ms);

    REQUIRE(m.metric_type() == ((internal::metric *)&n)->metric_type());
    INFO("metric_type: " << m.metric_type());
}

TEST_CASE("Meter copy assignment works", "[meter]")
{
    meter<1_min, 30_min, 7_day, 1_day, 1_hour> m(5ms);
    meter<1_min, 30_min, 7_day, 1_day, 1_hour> n(m);

    m = n;
}

TEST_CASE("Meter initializes correctly", "[meter]")
{
    meter<1_min, 1_sec> m(5ms);
    REQUIRE(m.rate<1_min>().rate == 0);
    REQUIRE(m.rate<1_sec>().rate == 0);
    REQUIRE(m.mean() == 0);

    auto ss = m.snapshot();
    REQUIRE(ss.value() == metric_value(0));
}

TEST_CASE("Meter rates are passed on", "[meter]")
{
    int clock = 0;
    mock_clock clk(clock);

    internal::_meter_impl<mock_clock, 1, 8, 20, 50> m(1, clk);

    for(int i = 0; i < 100; i++)
    {
        m.mark(10);
        clock++;
    }

    REQUIRE(round(m.template get_rate<1>()) == 10);
    REQUIRE(round(m.template get_rate<8>()) == 10);
    REQUIRE(round(m.template get_rate<20>()) == 10);
    REQUIRE(round(m.template get_rate<50>()) == 10);
    REQUIRE(round(m.mean()) == 10);

    clock += 10;
    m.mark(100);
    clock += 1;
    m.mark(0);

    REQUIRE(m.template get_rate<1>() > m.template get_rate<8>());
    REQUIRE(m.template get_rate<8>() > m.template get_rate<20>());
    REQUIRE(m.template get_rate<20>() > m.template get_rate<50>());
    REQUIRE_THAT(m.mean(), Catch::WithinULP(1100.0 / 111.0, 1));
}

TEST_CASE("Meter snapshot", "[meter]")
{
    meter<1_min, 30_min, 7_day, 1_day, 1_hour> m(5us);
    m.mark(100);
    std::this_thread::sleep_for(10us);

    auto ss = m.snapshot();
    for (const auto& pair : ss) {
        REQUIRE(pair.second != metric_value(0.0));
    }

    REQUIRE(ss.value() != metric_value(0.0));
}

