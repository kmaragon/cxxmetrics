#include <catch.hpp>
#include <ewma.hpp>
#include <thread>
#include "helpers.hpp"

using namespace std::chrono_literals;
using namespace cxxmetrics;
using namespace cxxmetrics_literals;

template<period::value TWindow, period::value TInterval>
using mock_ewma = internal::ewma<mock_clock, TWindow, TInterval>;

TEST_CASE("EWMA Initializes properly", "[ewma]")
{
    unsigned clock = 5;
    mock_ewma<30, 1> e(clock);

    REQUIRE(e.rate() == 0);
}

TEST_CASE("EWMA backwards clock skips", "[ewma]")
{
    unsigned clock = 5;
    mock_ewma<30, 1> e(clock);

    e.mark(1);

    clock = 2;
    e.mark(4);

    REQUIRE(e.rate() == 1);
}

TEST_CASE("EMWA calculates fixed rate", "[ewma]")
{
    unsigned clock = 1;
    mock_ewma<10, 1> e(clock);

    for (int i = 0; i <= 10; i++)
    {
        e.mark(7);
        clock++;
    }

    REQUIRE(round(e.rate()) == 7);
}

TEST_CASE("EWMA calculates fixed rate threads", "[ewma]")
{
    static constexpr auto interval = 1_msec;
    static constexpr int loopcnt = 100;
    ewma<100_msec, interval> e;

    double rate = 0;
    std::vector<std::thread> threads;

    auto start = std::chrono::steady_clock::now();
    threads.reserve(8);
    for (int i = 0; i < 8; i++)
    {
        threads.emplace_back([&e, &rate]() {
            for (int x = 0; x < loopcnt; x++)
            {
                std::this_thread::sleep_for(interval.to_duration());
                e.mark(5);
                rate = e.rate();
            }
        });
    }

    for (auto &thr : threads)
        thr.join();
    auto end = std::chrono::steady_clock::now();
    auto span = end - start;

    // each thread marks 5 per interval and there are n threads in a loop of 20
    auto total_iterations = loopcnt * threads.size();
    auto total_marks = 5 * total_iterations;
    auto actual = (std::chrono::seconds(1) * total_marks) / span;

    // note to reader: this is not a benchmark. This is just a sanity check for a 10ms windowed ewma
    INFO("Reported: " << rate << " (" << (rate * 1000) << " marks per second), Actual: " << actual << " marks per second");
    REQUIRE(rate >= 5.0);
}

TEST_CASE("EWMA calculates after jump past window", "[ewma]")
{
    unsigned clock = 1;
    mock_ewma<10, 1> e(clock);

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
    unsigned clock = 1;
    mock_ewma<10, 1> e(clock);

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
