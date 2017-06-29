#include <gtest/gtest.h>
#include <string>
#include <thread>
#include <mutex>

#include <execinfo.h>
#include "skiplist.hpp"

using namespace std;
using namespace cxxmetrics::internal;

TEST(skiplist_test, insert_head)
{
    skiplist<double> list;

    list.insert(8.9988);

    std::vector<double> values(list.begin(), list.end());
    ASSERT_EQ(values.size(), 1);
    ASSERT_DOUBLE_EQ(values[0], 8.9988);
}

TEST(skiplist_test, insert_additional)
{
    skiplist<double> list;

    list.insert(8.9988);
    list.insert(15.6788);
    list.insert(8000);
    list.insert(1000.4050001);
    list.insert(5233.05);

    std::vector<double> values(list.begin(), list.end());
    ASSERT_EQ(values.size(), 5);
    ASSERT_DOUBLE_EQ(values[0], 8.9988);
    ASSERT_DOUBLE_EQ(values[1], 15.6788);
    ASSERT_DOUBLE_EQ(values[2], 1000.4050001);
    ASSERT_DOUBLE_EQ(values[3], 5233.05);
    ASSERT_DOUBLE_EQ(values[4], 8000);

    std::vector<double> reverse(list.rbegin(), list.rend());
    ASSERT_EQ(reverse.size(), 5);
    ASSERT_DOUBLE_EQ(reverse[0], 8000);
    ASSERT_DOUBLE_EQ(reverse[1], 5233.05);
    ASSERT_DOUBLE_EQ(reverse[2], 1000.4050001);
    ASSERT_DOUBLE_EQ(reverse[3], 15.6788);
    ASSERT_DOUBLE_EQ(reverse[4], 8.9988);
}

TEST(skiplist_test, insert_duplicate)
{
    skiplist<double> list;

    list.insert(8.9988);
    list.insert(15.6788);
    list.insert(8.9988);
    list.insert(5233.05);

    std::vector<double> values(list.begin(), list.end());
    ASSERT_EQ(values.size(), 3);
    ASSERT_DOUBLE_EQ(values[0], 8.9988);
    ASSERT_DOUBLE_EQ(values[1], 15.6788);
    ASSERT_DOUBLE_EQ(values[2], 5233.05);

    std::vector<double> reverse(list.rbegin(), list.rend());
    ASSERT_EQ(reverse.size(), 3);
    ASSERT_DOUBLE_EQ(reverse[0], 5233.05);
    ASSERT_DOUBLE_EQ(reverse[1], 15.6788);
    ASSERT_DOUBLE_EQ(reverse[2], 8.9988);
}

TEST(skiplist_test, insert_lower)
{
    skiplist<double> list;

    list.insert(8000);
    list.insert(1000.4050001);
    list.insert(5233.05);
    list.insert(8.9988);
    list.insert(15.6788);

    std::vector<double> values(list.begin(), list.end());
    ASSERT_EQ(values.size(), 5);
    ASSERT_DOUBLE_EQ(values[0], 8.9988);
    ASSERT_DOUBLE_EQ(values[1], 15.6788);
    ASSERT_DOUBLE_EQ(values[2], 1000.4050001);
    ASSERT_DOUBLE_EQ(values[3], 5233.05);
    ASSERT_DOUBLE_EQ(values[4], 8000);

    std::vector<double> reverse(list.rbegin(), list.rend());
    ASSERT_EQ(reverse.size(), 5);
    ASSERT_DOUBLE_EQ(reverse[0], 8000);
    ASSERT_DOUBLE_EQ(reverse[1], 5233.05);
    ASSERT_DOUBLE_EQ(reverse[2], 1000.4050001);
    ASSERT_DOUBLE_EQ(reverse[3], 15.6788);
    ASSERT_DOUBLE_EQ(reverse[4], 8.9988);
}

TEST(skiplist_test, insert_only_threads)
{
    skiplist<double> list;
    atomic_uint_fast64_t at(0);
    vector<thread> workers;

    for (int i = 0; i < 16; i++)
    {
        workers.emplace_back([&]() {
            while (true)
            {
                auto mult = at.fetch_add(1);
                if (mult >= 5000)
                    return;

                if (mult % 2)
                    std::this_thread::yield();
                double insert = (0.17 * mult);
                list.insert(insert);
            }
        });
    }

    for (auto &thr : workers)
        thr.join();

    std::vector<double> values(list.begin(), list.end());
    ASSERT_EQ(values.size(), 5000);
    for (int x = 0; x < 5000; x++)
        ASSERT_DOUBLE_EQ(values[x], 0.17 * x);

    std::vector<double> reverse(list.rbegin(), list.rend());
    ASSERT_EQ(reverse.size(), 5000);
    for (int x = 1; x <= 5000; x++)
        ASSERT_DOUBLE_EQ(reverse[5000 - x], 0.17 * (x - 1));
}
