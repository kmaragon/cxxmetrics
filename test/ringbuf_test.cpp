#include <catch2/catch.hpp>
#include <cxxmetrics/ringbuf.hpp>

using namespace std;
using namespace cxxmetrics::internal;

TEST_CASE("Ringbuf can push full circle", "[ringbuf]")
{
    ringbuf<double, 6> subject;

    subject.push(12);
    subject.push(15.33);
    subject.push(18.21);
    subject.push(19.001);
    subject.push(8.9);
    subject.push(120000.0001);
    subject.push(1);
    subject.push(-99);
    subject.push(-91080);
    subject.push(1558771.05);

    REQUIRE(subject.size() == 6);
    std::vector<double> values;
    values.assign(subject.begin(), subject.end());

    REQUIRE(values.size() == 6);
    REQUIRE_THAT(values[5], Catch::WithinULP(1558771.05, 1));
    REQUIRE_THAT(values[4], Catch::WithinULP(-91080.0, 1));
    REQUIRE_THAT(values[3], Catch::WithinULP(-99.0, 1));
    REQUIRE_THAT(values[2], Catch::WithinULP(1.0, 1));
    REQUIRE_THAT(values[1], Catch::WithinULP(120000.0001, 1));
    REQUIRE_THAT(values[0], Catch::WithinULP(8.9, 1));
}
