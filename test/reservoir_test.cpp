#include <gtest/gtest.h>
#include <uniform_reservoir.hpp>

using namespace std;
using namespace cxxmetrics;

TEST(reservoir_test, uniform_reservoir_on_exact_count)
{
    uniform_reservoir<double, 5> r;

    r.update(10.0);
    r.update(15.0);
    r.update(30.0);
    r.update(40.0);
    r.update(45.0);

    auto s = r.snapshot();

    ASSERT_DOUBLE_EQ(s.min(), 10.0);
    ASSERT_DOUBLE_EQ(s.max(), 45.0);
    ASSERT_TRUE((s.template value<99_p>() - 45.0) < 1) << "Expected P99 to be about 45 but it was " << s.template value<99_p>();
    ASSERT_TRUE((s.template value<55_p>() - 35.0) < 1) << "Expected P99 to be about 35 but it was " << s.template value<55_p>();
    ASSERT_DOUBLE_EQ(s.mean(), 28.0);
}
