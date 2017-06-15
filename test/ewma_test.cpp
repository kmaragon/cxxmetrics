#include <gtest/gtest.h>
#include <ewma.hpp>
#include <thread>

using namespace std::chrono_literals;
using namespace cxxmetrics;

struct mock_clock
{
    mock_clock(int &ref) : value_(ref)
    {
    }

    int operator()() const
    {
        return value_;
    }

private:
    int &value_;
};

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

    for (int i = 0; i <= 20; i++)
    {
        clock++;
        e.mark(7);
    }

    ASSERT_DOUBLE_EQ(e.rate(), 7.0);
}

TEST(ewma_test, calculates_fixed_rate_threads)
{
    ewma e(10ms, 2ms);

    std::vector<std::thread> threads;

    for (int i = 0; i <= 20; i++)
    {
        threads.emplace_back([&e]() {
            for (int x = 0; x < 10; x++)
            {
                e.mark(7);
                std::this_thread::sleep_for(2ms);
            }
        });
    }

    for (auto &thr : threads)
        thr.join();

    auto rate = e.rate();
    std::cout << rate << std::endl;
    ASSERT_GE(rate, 140.0);
}

TEST(ewma_test, calculates_after_jump_past_window)
{
    int clock = 0;
    mock_ewma e(10, 1, clock);

    for (int i = 0; i <= 20; i++)
    {
        clock++;
        e.mark(7);
    }

    ASSERT_DOUBLE_EQ(e.rate(), 7.0);

    clock += 100;
    e.mark(1);
    ASSERT_LT(e.rate(), 1);
}

TEST(ewma_test, calculates_after_jump_in_window)
{
    int clock = 0;
    mock_ewma e(10, 1, clock);

    for (int i = 0; i <= 20; i++)
    {
        clock++;
        e.mark(7);
    }

    ASSERT_DOUBLE_EQ(e.rate(), 7.0);

    clock += 9;
    e.mark(1);
    ASSERT_LT(e.rate(), 1);
}

TEST(ewma_test, produces_correct_type)
{
    ewma a(5s);

    ASSERT_EQ(a.metric_type(), "cxxmetrics::ewma");
}