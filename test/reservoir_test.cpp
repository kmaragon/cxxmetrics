#include <gtest/gtest.h>
#include <simple_reservoir.hpp>
#include <uniform_reservoir.hpp>
#include <sliding_window.hpp>
#include "helpers.hpp"

using namespace std;
using namespace cxxmetrics;

TEST(reservoir_test, uniform_reservoir_on_exact_count)
{
    uniform_reservoir<double, 5> r;

    r.update(10.0);
    r.update(15.0);
    r.update(30.0);
    r.update(40.0);
    r.update(45.0);

    auto s = r.snapshot();

    ASSERT_DOUBLE_EQ(s.min(), 10.0);
    ASSERT_DOUBLE_EQ(s.max(), 45.0);
    ASSERT_TRUE(abs(s.value<99_p>() - 45.0) < 1) << "Expected P99 to be about 45 but it was " << s.value<99_p>();
    ASSERT_TRUE(abs(s.value<60_p>() - 35.0) <= 1) << "Expected P60 to be about 35 but it was " << s.value<60_p>();
    ASSERT_DOUBLE_EQ(s.mean(), 28.0);

    uniform_reservoir<double, 5> q = r;
}

TEST(reservoir_test, uniform_reservoir_with_overflow)
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

    ASSERT_TRUE(abs(min - 100) < 20) << "Expected min to be around 100 bit it was " << min;
    ASSERT_TRUE(abs(max - 200) < 20) << "Expected max to be around 200 bit it was " << max;
    ASSERT_TRUE(abs(mean - 150) < 20) << "Expected mean to be around 150 bit it was " << mean;
    ASSERT_TRUE(abs(p50 - 150) < 20) << "Expected P50 to be around 150 bit it was " << p50;
}

TEST(reservoir_test, simple_reservoir_overflow)
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

    ASSERT_DOUBLE_EQ(s.min(), 10.0);
    ASSERT_DOUBLE_EQ(s.max(), 45.0);
    ASSERT_TRUE(abs(s.value<99_p>() - 45.0) < 1) << "Expected P99 to be about 45 but it was " << s.value<99_p>();
    ASSERT_TRUE(abs(s.value<60_p>() - 35.0) <= 1) << "Expected P60 to be about 35 but it was " << s.value<60_p>();
    ASSERT_DOUBLE_EQ(s.mean(), 28.0);

    simple_reservoir<double, 5> q = r;
}

TEST(reservoir_test, sliding_window_reservoir_only_gets_window_data)
{
    int time = 500;
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

    ASSERT_DOUBLE_EQ(s.min(), 20.0);
    ASSERT_DOUBLE_EQ(s.max(), 60.0);
    ASSERT_TRUE(abs(s.value<99_p>() - 60.0) < 1) << "Expected P99 to be about 60 but it was " << s.value<99_p>();
    ASSERT_TRUE(abs(s.value<60_p>() - 40.0) <= 1) << "Expected P60 to be about 40 but it was " << s.value<60_p>();
    ASSERT_DOUBLE_EQ(s.mean(), 37.5);

    sliding_window_reservoir<double, 10, mock_clock> q = r;
}