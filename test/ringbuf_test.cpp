#include <catch.hpp>
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

TEST_CASE("Ringbuf can iterate during push", "[ringbuf]")
{
    ringbuf<double, 5> subject;

    subject.push(12);

    auto b = subject.begin();
    REQUIRE(*b == 12);

    subject.push(15.33);
    subject.push(18.21);
    ++b;
    REQUIRE(*b == 15.33);

    subject.push(19.001);
    subject.push(8.9);
    subject.push(120000.0001);
    subject.push(1);
    subject.push(-99);
    subject.push(-91080);

    ++b;
    REQUIRE(*b == 8.9);
}

TEST_CASE("Ringbuf can shift the whole array", "[ringbuf]")
{
    ringbuf<double, 5> subject;

    subject.push(12);
    subject.push(15.33);
    subject.push(18.21);
    subject.push(19.001);
    subject.push(8.9);

    REQUIRE(subject.size() == 5);
    REQUIRE_THAT(subject.shift(), Catch::WithinULP(12.0, 1));
    REQUIRE_THAT(subject.shift(), Catch::WithinULP(15.33, 1));
    REQUIRE_THAT(subject.shift(), Catch::WithinULP(18.21, 1));

    REQUIRE(subject.size() == 2);
    REQUIRE_THAT(subject.shift(), Catch::WithinULP(19.001, 1));
    REQUIRE_THAT(subject.shift(), Catch::WithinULP(8.9, 1));

    REQUIRE_THAT(subject.size(), Catch::WithinULP(0.0, 1));
}
