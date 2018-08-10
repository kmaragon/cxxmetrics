#include <catch.hpp>
#include <cxxmetrics/internal/atomic_lifo.hpp>
#include <set>
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

TEST_CASE("Atomic Lifo can push and pop", "[atomic_lifo]")
{
    atomic_lifo<int> p;
    p.emplace(5);
    p.push(7);
    p.push(9);

    REQUIRE(*p.pop() == 9);
    REQUIRE(*p.pop() == 7);
    REQUIRE(*p.pop() == 5);
}

TEST_CASE("Atomic Lifo initializer list construction", "[atomic_lifo]")
{
    atomic_lifo<long> p = {45L, 9000L, 81000L, 9900000L};

    REQUIRE(*p.pop() == 45L);
    REQUIRE(*p.pop() == 9000L);
    REQUIRE(*p.pop() == 81000L);
    REQUIRE(*p.pop() == 9900000L);
}

TEST_CASE("Atomic Lifo custom node is respected", "[atomic_lifo]")
{
    atomic_lifo<test_node> p;
    p.emplace(5);
    p.emplace(7);
    p.emplace(9);

    auto current = p.pop();
    REQUIRE(current->value() == 9);
    REQUIRE(current->had_next_set());

    current = p.pop();
    REQUIRE(current->value() == 7);
    REQUIRE(current->had_next_set());

    current = p.pop();
    REQUIRE(current->value() == 5);
}

TEST_CASE("Atomic Lifo custom node can be recycled", "[atomic_lifo]")
{
    atomic_lifo<test_node> p;
    p.emplace(5);
    p.emplace(7);
    p.emplace(9);

    auto current = p.pop();
    REQUIRE(current->value() == 9);
    REQUIRE(current->had_next_set());

    current = p.pop();
    REQUIRE(current->value() == 7);
    REQUIRE(current->had_next_set());

    p.push(std::move(current));
    REQUIRE(!current);
    current = p.pop();
    REQUIRE(current->value() == 7);
    REQUIRE(current->had_next_set());

    current = p.pop();
    REQUIRE(current->value() == 5);
}

TEST_CASE("Atomic Lifo multithreaded works", "[atomic_lifo]")
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
        REQUIRE(value == expected);
        expected++;
    }

    REQUIRE(expected == 1600);
}

TEST_CASE("Atomic Lifo multithreaded works with custom node", "[atomic_lifo]")
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
        REQUIRE(value == expected);
        expected++;
    }

    REQUIRE(expected == 1600);
}
