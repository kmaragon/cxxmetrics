#include <gtest/gtest.h>
#include <meter.hpp>
#include <thread>
#include <ctti/type_id.hpp>
#include "helpers.hpp"

using namespace std;
using namespace std::chrono_literals;
using namespace cxxmetrics;

TEST(meter_test, parameters_with_mean_get_sorted_for_equal_types)
{
    meter_with_mean<1_min, 30_min, 7_day, 1_day, 1_hour> m(5ms);
    meter_with_mean<1_hour, 30_min, 1_day, 7_day, 7_day, 30_min, 1_min> n(5ms);

    ASSERT_EQ(m.metric_type(), ((internal::metric *)&n)->metric_type());
    cout << "metric_type: " << m.metric_type() << endl;
}

TEST(meter_test, parameters_without_mean_get_sorted_for_equal_types)
{
    meter_rates_only<1_min, 30_min, 7_day, 1_day, 1_hour> m(5ms);
    meter_rates_only<1_hour, 30_min, 1_day, 7_day, 7_day, 30_min, 1_min> n(5ms);

    ASSERT_EQ(m.metric_type(), ((internal::metric *)&n)->metric_type());
    cout << "metric_type: " << m.metric_type() << endl;
}

TEST(meter_test, copy_assignment_works)
{
    meter_with_mean<1_min, 30_min, 7_day, 1_day, 1_hour> m(5ms);
    meter_with_mean<1_min, 30_min, 7_day, 1_day, 1_hour> n(m);

    m = n;
}

TEST(meter_test, all_rates_are_passed_on)
{
    int clock;
    mock_clock clk(clock);

    internal::_meter_impl<true, mock_clock, 1, 8, 20, 50> m(1, clk);

    for(int i = 0; i < 100; i++)
    {
        m.mark(10);
        clock++;
    }

    ASSERT_EQ(round(m.template get_rate<1>() * 100), 1000);
    ASSERT_EQ(round(m.template get_rate<8>() * 100), 1000);
    ASSERT_EQ(round(m.template get_rate<20>() * 100), 1000);
    ASSERT_EQ(round(m.template get_rate<50>() * 100), 1000);
    ASSERT_EQ(round(m.mean() * 100), 1000);

    clock += 10;
    m.mark(100);
    clock += 1;
    m.mark(0);

    ASSERT_GT(m.template get_rate<1>(), m.template get_rate<8>());
    ASSERT_GT(m.template get_rate<8>(), m.template get_rate<20>());
    ASSERT_GT(m.template get_rate<20>(), m.template get_rate<50>());
    ASSERT_DOUBLE_EQ(m.mean(), 1100.0 / 111.0);
}