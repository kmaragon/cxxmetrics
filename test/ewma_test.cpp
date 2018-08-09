#include <catch.hpp>
#include <ewma.hpp>
#include <thread>
#include "helpers.hpp"

using namespace std::chrono_literals;
using namespace cxxmetrics;
using namespace cxxmetrics_literals;

using mock_ewma = internal::ewma<mock_clock>;

TEST_CASE("EWMA Initializes properly", "[ewma]")
{
    int clock = 5;
    mock_ewma e(30, 1, clock);

    REQUIRE(e.rate() == 0);
}

TEST_CASE("EWMA backwards clock skips", "[ewma]")
{
    int clock = 5;
    mock_ewma e(30, 1, clock);

    e.mark(1);

    clock = 2;
    e.mark(4);

    REQUIRE(e.rate() == 1);
}

TEST_CASE("EMWA calculates fixed rate", "[ewma]")
{
    int clock = 1;
    mock_ewma e(10, 1, clock);

    for (int i = 0; i <= 10; i++)
    {
        e.mark(7);
        clock++;
    }

    REQUIRE(round(e.rate()) == 7);
}

TEST_CASE("EWMA calculates fixed rate threads", "[ewma]")
{
    ewma<10_msec, 2_msec> e;

    double rate;
    std::vector<std::thread> threads;

    for (int i = 0; i <= 10; i++)
    {
        threads.emplace_back([&e, &rate]() {
            for (int x = 0; x < 10; x++)
            {
                e.mark(5);
                rate = e.rate();
                std::this_thread::sleep_for(1ms);
            }
        });
    }

    for (auto &thr : threads)
        thr.join();

    // note to reader: this is not a benchmark. This is just a sanity check for a 10ms windowed ewma
    INFO(rate << " (" << (rate * 100) << " marks per second)");
    REQUIRE(rate >= 5.0);
}

TEST_CASE("EWMA calculates after jump past window", "[ewma]")
{
    int clock = 1;
    mock_ewma e(10, 1, clock);

    for (int i = 0; i <= 10; i++)
    {
        e.mark(7);
        clock++;
    }

    REQUIRE(round(e.rate()) == 7);

    clock += 100;
    e.mark(1);
    REQUIRE(e.rate() <= 1);
}

TEST_CASE("EWMA calculates after jump in window", "[ewma]")
{
    int clock = 1;
    mock_ewma e(10, 1, clock);

    for (int i = 0; i <= 100; i++)
    {
        clock++;
        e.mark(7);
    }

    REQUIRE(round(e.rate() * 100) == 700);

    clock += 40;
    e.mark(1);
    REQUIRE(e.rate() <= 1);
}

TEST_CASE("EWMA produces correct type", "[ewma]")
{
    ewma<5_sec, 5_sec, double> a;
    ewma<5_sec, 10_sec, double> b;

    REQUIRE(a.metric_type() != b.metric_type());
}

TEST_CASE("EWMA excercise snapshot", "[ewma]")
{
    ewma<10_sec, 5_sec, double> e;
    REQUIRE(e.snapshot() == 0);
}
