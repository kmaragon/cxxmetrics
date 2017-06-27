#include <gtest/gtest.h>
#include <ringbuf.hpp>

using namespace std;
using namespace cxxmetrics::internal;

TEST(ringbuf_test, can_push_full_circle)
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

    ASSERT_EQ(subject.size(), 6);
    std::vector<double> values;
    values.assign(subject.begin(), subject.end());

    ASSERT_EQ(values.size(), 6);
    ASSERT_DOUBLE_EQ(values[5], 1558771.05);
    ASSERT_DOUBLE_EQ(values[4], -91080);
    ASSERT_DOUBLE_EQ(values[3], -99);
    ASSERT_DOUBLE_EQ(values[2], 1);
    ASSERT_DOUBLE_EQ(values[1], 120000.0001);
    ASSERT_DOUBLE_EQ(values[0], 8.9);
}

TEST(ringbuf_test, can_iterate_during_push)
{
    ringbuf<double, 5> subject;

    subject.push(12);

    auto b = subject.begin();
    ASSERT_EQ(*b, 12);

    subject.push(15.33);
    subject.push(18.21);
    ++b;
    ASSERT_EQ(*b, 15.33);

    subject.push(19.001);
    subject.push(8.9);
    subject.push(120000.0001);
    subject.push(1);
    subject.push(-99);
    subject.push(-91080);

    ++b;
    ASSERT_EQ(*b, 8.9);
}

TEST(ringbuf_test, can_shift_the_whole_array)
{
    ringbuf<double, 5> subject;

    subject.push(12);
    subject.push(15.33);
    subject.push(18.21);
    subject.push(19.001);
    subject.push(8.9);

    ASSERT_EQ(subject.size(), 5);
    ASSERT_DOUBLE_EQ(subject.shift(), 12);
    ASSERT_DOUBLE_EQ(subject.shift(), 15.33);
    ASSERT_DOUBLE_EQ(subject.shift(), 18.21);

    ASSERT_EQ(subject.size(), 2);
    ASSERT_DOUBLE_EQ(subject.shift(), 19.001);
    ASSERT_DOUBLE_EQ(subject.shift(), 8.9);

    ASSERT_DOUBLE_EQ(subject.size(), 0);
}