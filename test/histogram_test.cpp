#include <catch.hpp>
#include <histogram.hpp>
#include <simple_reservoir.hpp>

using namespace cxxmetrics;
using namespace cxxmetrics::literals;

TEST_CASE("Histogram sanity check", "[histogram]")
{
    histogram<double, simple_reservoir<double, 5>> h;

    h.update(200);
    h.update(10);
    h.update(13);
    h.update(10.0);
    h.update(15.0);
    h.update(30.0);
    h.update(40.0);
    h.update(45.0);

    auto s = h.snapshot();

    REQUIRE_THAT(s.min(), Catch::WithinULP(10.0, 1));
    REQUIRE_THAT(s.max(), Catch::WithinULP(45.0, 1));
    REQUIRE(abs(static_cast<double>(s.value<99_p>()) - 45.0) < 1);
    REQUIRE(abs(static_cast<double>(s.value<60_p>()) - 35.0) <= 1);
    REQUIRE_THAT(s.mean(), Catch::WithinULP(28.0, 1));
    REQUIRE(s.count() == 8);
}
