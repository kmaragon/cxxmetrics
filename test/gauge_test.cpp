#include <gtest/gtest.h>
#include <gauge.hpp>

using namespace std;
using namespace cxxmetrics;

TEST(gauge_test, primitive_gauge_works)
{
    gauge<std::string> g("hola");
    ASSERT_EQ(g.get(), "hola");

    g.set("hello");
    ASSERT_EQ(g.get(), "hello");

    gauge<int> h;
    ASSERT_EQ((h = 20).get(), 20);

    h.set(50);
    ASSERT_EQ(h.get(), 50);
}

TEST(gauge_test, functional_gauge_works)
{
    double value = 99.810;
    std::function<double()> fn = [&value]() { return value; };

    gauge<decltype(fn)> g(fn);
    ASSERT_DOUBLE_EQ(g.get(), 99.81);

    value = 10000;
    ASSERT_DOUBLE_EQ(g.get(), 10000);
}

TEST(gauge_test, pointer_gauge_works)
{
    float v = 70.0f;
    gauge<float *> g(&v);
    ASSERT_FLOAT_EQ(g.get(), 70);

    v = 500.017f;
    ASSERT_FLOAT_EQ(g.get(), 500.017);

    float x = 0;
    gauge<float *> h(&x);
    ASSERT_FLOAT_EQ((g = h).get(), 0);
}

TEST(gauge_test, reference_gauge_works)
{
    char v = 'A';
    gauge<char &> g(v);
    ASSERT_FLOAT_EQ(g.get(), 'A');

    v = 'z';
    ASSERT_FLOAT_EQ(g.get(), 'z');
}