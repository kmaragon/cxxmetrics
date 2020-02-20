#include <catch2/catch.hpp>
#include <cxxmetrics/timer.hpp>
#include <cxxmetrics/simple_reservoir.hpp>

using namespace cxxmetrics;
using namespace cxxmetrics_literals;
using namespace std::chrono_literals;

TEST_CASE("Timer Tests", "[timer]")
{
    using timer_t = timer<100_micro, std::chrono::system_clock, simple_reservoir<std::chrono::system_clock::duration, 4>, true, 5_min, 1_min, 10_sec>;

    timer_t t;
    SECTION("Constant Timers snapshots produce expected results")
    {
        t.update(std::chrono::microseconds(1000));
        t.update(std::chrono::microseconds(10));
        t.update(std::chrono::microseconds(20));
        t.update(std::chrono::microseconds(40));
        t.update(std::chrono::microseconds(80));

        auto ss = t.snapshot();
        REQUIRE(std::chrono::duration_cast<std::chrono::microseconds>(ss.min()).count() == 10);
        REQUIRE(std::chrono::duration_cast<std::chrono::microseconds>(ss.value<10_p>()).count() == 10);
        REQUIRE(std::chrono::duration_cast<std::chrono::microseconds>(ss.value<20_p>()).count() == 10);
        REQUIRE(std::chrono::duration_cast<std::chrono::microseconds>(ss.value<40_p>()).count() == 20);
        REQUIRE(std::chrono::duration_cast<std::chrono::microseconds>(ss.value<60_p>()).count() == 40);
        REQUIRE(std::chrono::duration_cast<std::chrono::microseconds>(ss.value<80_p>()).count() == 80);
        REQUIRE(std::chrono::duration_cast<std::chrono::microseconds>(ss.value<100_p>()).count() == 80);
        REQUIRE(std::chrono::duration_cast<std::chrono::microseconds>(ss.max()).count() == 80);

        REQUIRE(std::chrono::duration_cast<std::chrono::microseconds>(ss.mean()).count() == 37);
        REQUIRE(ss.count() == 5);
    }

    SECTION("Timer scopes time correctly")
    {
        for (int i = 0; i < 100; i++)
        {
            auto localt = scoped_timer(t);
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }

        auto ss = t.snapshot();
        for (auto& rate_pair : ss.rate())
        {
            REQUIRE(rate_pair.second > metric_value(0));
        }

        REQUIRE(std::abs(static_cast<double>(ss.rate().value()) - 1.0) < 0.5);
    }

    SECTION("Scope Transfer works")
    {

        t.update(std::chrono::seconds(1000));
        t.update(std::chrono::seconds(1000));
        t.update(std::chrono::seconds(1000));
        t.update(std::chrono::seconds(1000));
        t.update(std::chrono::seconds(1000));

        std::unique_ptr<scoped_timer_t<timer_t>> ptr;
        {
            auto localt = scoped_timer(t);
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            ptr = std::make_unique<scoped_timer_t<timer_t>>(std::move(localt));
        }

        auto ss = t.snapshot();

        // make sure the new shorter time didn't just get inserted
        REQUIRE(std::chrono::duration_cast<std::chrono::microseconds>(ss.min()).count() == 1000000000);

        ptr.reset(nullptr);

        ss = t.snapshot();

        // now let the transferred pointer finish and the min should have dropped
        REQUIRE(std::chrono::duration_cast<std::chrono::microseconds>(ss.min()).count() < 100000000);
    }

}
