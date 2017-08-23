#ifndef CXXMETRICS_PQSKIPLIST_HPP
#define CXXMETRICS_PQSKIPLIST_HPP

#include <atomic>
#include <array>

namespace cxxmetrics
{
namespace internal
{

template<int TSize, int TBase = 2>
struct const_log
{
private:
    static constexpr int l = (TSize % TBase) ? TSize + (TBase - (TSize % TBase)) : TSize;

public:
    static_assert(TBase > 1, "const log is only useful for > 1");
    static constexpr int value = (const_log<l / TBase, TBase>::value + 1);
};

template<int TBase>
struct const_log<1, TBase>
{
    static constexpr int value = 0;
};

template<typename T, int TSize>
class skiplist_node
{
public:
    static constexpr int width = const_log<TSize>::value;
    using ptr = std::atomic<skiplist_node *>;
private:
    std::array<ptr, width> next_;
    T value_;
};

template<typename T, int TSize>
class skiplist
{
    using node_ptr = typename skiplist_node<T, TSize>::ptr;

    node_ptr head_;
public:
    static constexpr int width = skiplist_node<T, TSize>::width;

    skiplist() noexcept;
    void insert(const T &value) noexcept;
};

template<typename T, int TSize>
skiplist<T, TSize>::skiplist() :
        head_(nullptr)
{
}

}
}

#endif //CXXMETRICS_PQSKIPLIST_HPP
