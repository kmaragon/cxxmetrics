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

    ASSERT_NE(list.find(8.9988), list.end());
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

    ASSERT_NE(list.find(8.9988), list.end());
    ASSERT_NE(list.find(1000.4050001), list.end());
    ASSERT_NE(list.find(8000), list.end());

#ifdef CXXMETRICS_DEBUG
    for (int i = 0; i < 8; i++)
        list.dump_nodes(i);
#endif
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
}

TEST(skiplist_test, insert_only_threads)
{
    skiplist<double, 16> list;
    atomic_uint_fast64_t at(0);
    vector<thread> workers;

    for (int i = 0; i < 16; i++)
    {
        workers.emplace_back([&]() {
            while (true)
            {
                auto mult = at.fetch_add(1);
                if (mult >= 1000)
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
    ASSERT_EQ(values.size(), 1000);
    for (int x = 0; x < 1000; x++)
    {
        // assert on every 10th result
        if (!(x % 10))
            ASSERT_NE(list.find(0.17 * x), list.end());
        ASSERT_DOUBLE_EQ(values[x], 0.17 * x);
    }

#ifdef CXXMETRICS_DEBUG
    for (int i = 0; i < 8; i++)
        list.dump_nodes(i);
#endif
}

TEST(skiplist_test, erase_head_on_a_few)
{
    skiplist<double> list;

    list.insert(8000);
    list.insert(1000.4050001);
    list.insert(5233.05);
    list.insert(8.9988);
    list.insert(15.6788);

    list.erase(list.begin());

#ifdef CXXMETRICS_DEBUG
    for (int i = 0; i < 8; i++)
        list.dump_nodes(i);
#endif

    auto begin = list.begin();
    auto end = list.end();

    std::vector<double> values(list.begin(), list.end());
    ASSERT_EQ(values.size(), 4);
    ASSERT_DOUBLE_EQ(values[0], 15.6788);
    ASSERT_DOUBLE_EQ(values[1], 1000.4050001);
    ASSERT_DOUBLE_EQ(values[2], 5233.05);
    ASSERT_DOUBLE_EQ(values[3], 8000);
}

TEST(skiplist_test, erase_tail_on_a_few)
{
    skiplist<double> list;

    list.insert(8000);
    list.insert(1000.4050001);
    list.insert(5233.05);
    list.insert(8.9988);
    list.insert(15.6788);

    list.erase(list.find(8000));

#ifdef CXXMETRICS_DEBUG
    for (int i = 0; i < 8; i++)
        list.dump_nodes(i);
#endif

    auto begin = list.begin();
    auto end = list.end();

    std::vector<double> values(list.begin(), list.end());
    ASSERT_EQ(values.size(), 4);
    ASSERT_DOUBLE_EQ(values[0], 8.9988);
    ASSERT_DOUBLE_EQ(values[1], 15.6788);
    ASSERT_DOUBLE_EQ(values[2], 1000.4050001);
    ASSERT_DOUBLE_EQ(values[3], 5233.05);
}

TEST(skiplist_test, erase_mid_on_a_few)
{
    skiplist<double> list;

    list.insert(8000);
    list.insert(1000.4050001);
    list.insert(5233.05);
    list.insert(8.9988);
    list.insert(15.6788);

    list.erase(list.find(5233.05));

#ifdef CXXMETRICS_DEBUG
    for (int i = 0; i < 8; i++)
        list.dump_nodes(i);
#endif

    auto begin = list.begin();
    auto end = list.end();

    std::vector<double> values(list.begin(), list.end());
    ASSERT_EQ(values.size(), 4);
    ASSERT_DOUBLE_EQ(values[0], 8.9988);
    ASSERT_DOUBLE_EQ(values[1], 15.6788);
    ASSERT_DOUBLE_EQ(values[2], 1000.4050001);
    ASSERT_DOUBLE_EQ(values[3], 8000);
}

TEST(skiplist_test, invalidated_iterator_still_works)
{
    skiplist<double> list;

    list.insert(8000);
    list.insert(5233.05);
    list.insert(8.9988);

    auto begin = list.begin();
    ASSERT_NE(begin, list.end());
    ASSERT_EQ(*begin, 8.9988);

    list.insert(15.6788);
    ++begin;
    ASSERT_NE(begin, list.end());
    ASSERT_EQ(*begin, 15.6788);

    ++begin;
    ASSERT_NE(begin, list.end());
    ASSERT_EQ(*begin, 5233.05);

    list.insert(10000.4050001);
    ++begin;
    ASSERT_NE(begin, list.end());
    ASSERT_EQ(*begin, 8000);

    list.erase(list.find(8000));
    ++begin;
    ASSERT_NE(begin, list.end());
    ASSERT_EQ(*begin, 10000.4050001);
}

TEST(skiplist_test, insert_and_delete_threads)
{
    skiplist<double, 16> list;
    atomic_uint_fast64_t at(0);
    vector<thread> workers;

    for (int i = 0; i < 32; i++)
    {
        workers.emplace_back([&]() {
            while (true)
            {
                auto mult = at.fetch_add(1);
                if (mult >= 1000)
                    return;

                if ((mult % 5) == 4)
                {
                    while (!list.erase(list.find(0.17 * (mult - 4))))
                        std::this_thread::yield();
                }
                else
                    list.insert(0.17 * mult);
            }
        });
    }

    for (auto &thr : workers)
        thr.join();

    std::vector<double> values(list.begin(), list.end());
    // we should have chopped out 40% of the list. in 20% of
    // the calls we deleted one item.
    for (int x = 0; x < 1000; x++)
    {
        // we skipped every 5 entries at both 0 and 4 offsets
        if (((x % 5) == 4) || !(x % 5))
            continue;

        // map x to an offset into our trimmed vector
        int offset = x - (((x / 5) * 2) + 1);
        ASSERT_DOUBLE_EQ(values[offset], 0.17 * x);
    }

    ASSERT_EQ(values.size(), 600);

    /*
    std::vector<double> reverse(list.rbegin(), list.rend());
    ASSERT_EQ(reverse.size(), 1000);
    for (int x = 1; x <= 1000; x++)
        ASSERT_DOUBLE_EQ(reverse[1000 - x], 0.17 * (x - 1));
        */
}
