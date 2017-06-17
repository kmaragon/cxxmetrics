#include <gtest/gtest.h>
#include <counter.hpp>

using namespace cxxmetrics;

TEST(counter_test, incr_and_wrappers_work)
{
    counter<int64_t> a(15);
    a += 5;
    ASSERT_EQ(a, 20);

    ++a;
    ASSERT_EQ(a, 21);

    a -= 16;
    ASSERT_EQ(a, 5);

    --a;
    ASSERT_EQ(a, 4);

    a = 10;
    ASSERT_EQ(a, 10);

    ASSERT_EQ(a.metric_type().substr(0, 20), "cxxmetrics::counter<");
}
