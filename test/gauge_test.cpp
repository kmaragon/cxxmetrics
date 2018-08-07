#include <catch.hpp>
#include <gauge.hpp>

using namespace std;
using namespace cxxmetrics;

TEST_CASE("Primitive Gauge works", "[gauge]")
{
    gauge<std::string> g("hola");
    REQUIRE(g.get() == "hola");

    g.set("hello");
    REQUIRE(g.get() == "hello");
    REQUIRE(g.snapshot() == "hello");

    gauge<int> h;
    REQUIRE((h = 20).get() == 20);

    h.set(50);
    REQUIRE(h.get() == 50);
    REQUIRE(h.snapshot() == 50);
}

TEST_CASE("Functional Gauge works", "[gauge]")
{
    double value = 99.810;
    std::function<double()> fn = [&value]() { return value; };

    gauge<decltype(fn)> g(fn);
    REQUIRE(g.get() == 99.81);

    value = 10000;
    REQUIRE_THAT(g.get(), Catch::WithinULP(10000.0, 1));
    REQUIRE_THAT(g.snapshot().value(), Catch::WithinULP(10000.0, 1));
}

TEST_CASE("Pointer Gauge works", "[gauge]")
{
    float v = 70.0f;
    gauge<float *> g(&v);
    REQUIRE(g.get() == 70);

    v = 500.017f;
    REQUIRE_THAT(g.get(), Catch::WithinULP(500.017f, 1));
    REQUIRE_THAT(g.snapshot().value(), Catch::WithinULP(500.017f, 1));

    float x = 0;
    gauge<float *> h(&x);
    REQUIRE((g = h).get() == 0.0);
    REQUIRE(g.snapshot() == 0.0);
}

TEST_CASE("Reference Gauge works", "[gauge]")
{
    char v = 'A';
    gauge<char &> g{v};
    REQUIRE(g.get() == 'A');

    v = 'z';
    REQUIRE(g.get() == 'z');
    REQUIRE(g.snapshot() == 'z');
}
