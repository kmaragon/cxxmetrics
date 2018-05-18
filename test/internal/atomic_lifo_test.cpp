#include <gtest/gtest.h>
#include <internal/atomic_lifo.hpp>
#include <thread>

using namespace cxxmetrics::internal;

struct test_node {
    int value_;
    test_node* next_;
    bool nextset_;

    test_node(int val) :
        value_(val)
    {
        next_ = nullptr;
    }

    test_node* next()
    {
        return next_;
    }

    void set_next(test_node* next)
    {
        next_ = next;
        nextset_ = true;
    }

    operator int&()
    {
        return value_;
    }

    int value() const
    {
        return value_;
    }

    operator const int&() const
    {
        return value_;
    }

    bool had_next_set() const
    {
        return nextset_;
    }
};

TEST(atomic_lifo_test, can_push_and_pop)
{
    atomic_lifo<int> p;
    p.emplace(5);
    p.push(7);
    p.push(9);

    ASSERT_EQ(*p.pop(), 9);
    ASSERT_EQ(*p.pop(), 7);
    ASSERT_EQ(*p.pop(), 5);
}

TEST(atomic_lifo_test, initializer_list_construction)
{
    atomic_lifo<long> p = {45L, 9000L, 81000L, 9900000L};

    ASSERT_EQ(*p.pop(), 45L);
    ASSERT_EQ(*p.pop(), 9000L);
    ASSERT_EQ(*p.pop(), 81000L);
    ASSERT_EQ(*p.pop(), 9900000L);
}

TEST(atomic_lifo_test, custom_node_is_respected)
{
    atomic_lifo<test_node> p;
    p.emplace(5);
    p.emplace(7);
    p.emplace(9);

    auto current = p.pop();
    ASSERT_EQ(current->value(), 9);
    ASSERT_TRUE(current->had_next_set());

    current = p.pop();
    ASSERT_EQ(current->value(), 7);
    ASSERT_TRUE(current->had_next_set());

    current = p.pop();
    ASSERT_EQ(current->value(), 5);
}

TEST(atomic_lifo_test, custom_node_can_be_recycled)
{
    atomic_lifo<test_node> p;
    p.emplace(5);
    p.emplace(7);
    p.emplace(9);

    auto current = p.pop();
    ASSERT_EQ(current->value(), 9);
    ASSERT_TRUE(current->had_next_set());

    current = p.pop();
    ASSERT_EQ(current->value(), 7);
    ASSERT_TRUE(current->had_next_set());

    p.push(std::move(current));
    ASSERT_FALSE(current);
    current = p.pop();
    ASSERT_EQ(current->value(), 7);
    ASSERT_TRUE(current->had_next_set());

    current = p.pop();
    ASSERT_EQ(current->value(), 5);
}

TEST(atomic_lifo_test, multithreaded_works)
{
    atomic_lifo<int> p;

    std::vector<std::thread> threads;
    for (int i = 0; i < 16; i++)
    {
        threads.emplace_back([&p, ctr = i * 100]() {
            for (int x = 0; x < 100; x++)
                p.emplace(x + ctr);
        });
    }

    for (auto& thr : threads)
        thr.join();

    std::set<int> results;
    auto n = p.pop();

    do {
        results.insert(*n);
        n = p.pop();
    } while (n);

    int expected = 0;
    for (auto value : results)
    {
        ASSERT_EQ(value, expected);
        expected++;
    }

    ASSERT_EQ(expected, 1600);
}

TEST(atomic_lifo_test, multithreaded_works_with_custom_node)
{
    atomic_lifo<test_node> p;

    std::vector<std::thread> threads;
    for (int i = 0; i < 16; i++)
    {
        threads.emplace_back([&p, ctr = i * 100]() {
            for (int x = 0; x < 100; x++)
                p.emplace(x + ctr);
        });
    }

    for (auto& thr : threads)
        thr.join();

    std::set<int> results;
    auto n = p.pop();

    do {
        results.insert(n->value());
        n = p.pop();
    } while (n);

    int expected = 0;
    for (auto value : results)
    {
        ASSERT_EQ(value, expected);
        expected++;
    }

    ASSERT_EQ(expected, 1600);
}
