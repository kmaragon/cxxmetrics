#include <gtest/gtest.h>
#include <ewma.hpp>
#include <thread>
#include "helpers.hpp"

using namespace std::chrono_literals;
using namespace cxxmetrics;


using mock_ewma = internal::ewma<mock_clock>;

TEST(ewma_test, initializes_properly)
{
    int clock = 5;
    mock_ewma e(30, 1, clock);

    e.mark(1);
    ASSERT_EQ(0, e.rate());
}

TEST(ewma_test, backwards_clock_skips_)
{
    int clock = 5;
    mock_ewma e(30, 1, clock);

    e.mark(1);

    clock = 2;
    e.mark(4);

    ASSERT_EQ(e.rate(), 0);
}

TEST(ewma_test, calculates_fixed_rate)
{
    int clock = 0;
    mock_ewma e(10, 1, clock);

    for (int i = 0; i <= 10; i++)
    {
        e.mark(7);
        clock++;
    }

    ASSERT_EQ(round(e.rate() * 100), 700);
}

TEST(ewma_test, calculates_fixed_rate_threads)
{
    ewma e(10ms, 2ms);

    double rate;
    std::vector<std::thread> threads;

    for (int i = 0; i <= 10; i++)
    {
        threads.emplace_back([&e, &rate]() {
            for (int x = 0; x < 10; x++)
            {
                e.mark(7);
                rate = e.rate();
                std::this_thread::sleep_for(1ms);
            }
        });


    }

    for (auto &thr : threads)
        thr.join();

    std::cout << rate << std::endl;
    ASSERT_GE(rate, 7.0);
}

TEST(ewma_test, calculates_after_jump_past_window)
{
    int clock = 0;
    mock_ewma e(10, 1, clock);

    for (int i = 0; i <= 10; i++)
    {
        e.mark(7);
        clock++;
    }

    ASSERT_EQ(round(e.rate() * 100), 700);

    clock += 100;
    e.mark(1);
    ASSERT_LT(e.rate(), 1);
}

TEST(ewma_test, calculates_after_jump_in_window)
{
    int clock = 0;
    mock_ewma e(10, 1, clock);

    for (int i = 0; i <= 100; i++)
    {
        clock++;
        e.mark(7);
    }

    ASSERT_EQ(round(e.rate() * 100), 700);

    clock += 20;
    e.mark(1);
    ASSERT_LT(e.rate(), 1);
}

TEST(ewma_test, produces_correct_type)
{
    ewma a(5s);

    ASSERT_EQ(a.metric_type(), "cxxmetrics::ewma");
}