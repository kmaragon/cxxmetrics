#ifndef CXXMETRICS_SKIPLIST_HPP_HPP
#define CXXMETRICS_SKIPLIST_HPP_HPP

#define CXXMETRICS_DISABLE_POOLING

#include "pool.hpp"
#include <random>

namespace cxxmetrics
{

namespace internal
{

template<typename TValue, uint16_t TWidth = 8, typename TLess = std::less <TValue>>
class _skiplist_data
{
public:

    class node;
    using node_ptr = pool_ptr<node>;

    struct node
    {
        std::atomic_uint_fast16_t level;
        node_ptr previous;
        std::array<node_ptr, TWidth> next;
        TValue value;

        node() noexcept :
                level(0)
        {
            static_assert(TWidth < 32768, "Width can't be larger than 0x7fff");
        }

        node(const node &n) :
                value(n.value)
        {
            // we don't copy anything else. The list will take care of that
        }

        bool is_marked() const
        {
            return level.load() >> 15;
        }
    };

    TLess cmp_;
    node_ptr head_;
    node_ptr tail_;
    pool<node> node_pool;

    ~_skiplist_data()
    {
        auto cur = head_;
        while (cur)
        {
            // allow 'gc' to happen on our nodes
            cur->previous = nullptr;
            cur = cur->next[0];
        }
    }

    pool_ptr<node> create_node(int16_t level, const TValue &value) noexcept
    {
        auto ptr = node_pool.allocate();
        ptr->value = value;
        ptr->level = level;

        ptr->previous = nullptr;
        for (int i = 0; i < TWidth; i++)
            ptr->next[i] = nullptr;

        return std::move(ptr);
    }

    node_ptr next(int level, node_ptr &node) noexcept
    {
        node_ptr node2 = node->next[level];
        while (node2 && node2->is_marked())
        {
            node = help_delete(level, node2);
            node2 = node->next[level];
        }

        assert(!node2 || node2->value > node->value);
        return std::move(node2);
    }

    node_ptr scan_values(int level, node_ptr &less, const TValue &value)
    {
        auto more = next(level, less);
        while (more && cmp_(more->value, value))
        {
            less = more;
            more = next(level, less);
        }

        return std::move(more);
    }

    std::pair<node_ptr, node_ptr> find_insert_loc(const TValue &value, std::array<node_ptr, TWidth> &saved)
    {
        if (!cmp_(head_->value, value))
            return {nullptr, head_}; // we are below head

        node_ptr less = head_;
        node_ptr more = nullptr;

        for (int i = TWidth - 1; i >= 0; i--)
        {
            more = scan_values(i, less, value);
            saved[i] = less;
        }

        assert(!more || less->value < more->value);
        return std::make_pair(std::move(less), std::move(more));
    }

    node_ptr help_delete(int level, node_ptr &node) noexcept
    {

    }

    void set_previous(const node_ptr &hint, node_ptr &node)
    {

    }

    void set_tail_if_last(node_ptr &node)
    {

    }

};


/**
 * \brief An implementation of a non-locking skiplist, roughly inspired by 'Fast and Lock-Free Concurrent Priority Queues for Multi-Threaded Systems' by Sundell and Tsigas
 *
 * \tparam TValue the type of value in the skiplist
 * \tparam TWidth the width of the skipwidth. This will have a dramatic impact on memory usage
 * \tparam TLess the comparator to use for sorting the skiplist
 */
template<typename TValue, uint16_t TWidth = 8, typename TLess = std::less <TValue>>
class skiplist : protected ::cxxmetrics::internal::_skiplist_data<TValue, TWidth, TLess>
{
    using base = ::cxxmetrics::internal::_skiplist_data<TValue, TWidth, TLess>;
    using node = typename base::node;
    using node_ptr = typename base::node_ptr;

    pool<node_ptr> pool_;

    inline int random_level()
    {
        std::uniform_int_distribution<int> dst(0, TWidth - 1);
        return dst(rnd_);
    }

    static std::default_random_engine rnd_;
public:
    template<bool TFwd>
    class iterator : public std::iterator<std::bidirectional_iterator_tag, TValue>
    {
        skiplist *list_;
        node_ptr node_;
        bool end_;

        iterator(skiplist *list, node_ptr node) noexcept;

        friend class skiplist;
    public:
        iterator() noexcept;
        iterator(const iterator &) noexcept = default;
        ~iterator() = default;
        iterator &operator=(const iterator &other) noexcept = default;

        iterator &operator++() noexcept;
        iterator &operator--() noexcept;

        bool operator==(const iterator &other) const noexcept;
        bool operator!=(const iterator &other) const noexcept;

        const TValue *operator->() const noexcept;
        const TValue &operator*() const noexcept;
    };

    /**
     * \brief Insert an item into the skiplist
     *
     * \param value the value to insert into the list
     */
    void insert(const TValue &value) noexcept;
};

template<typename TValue, uint16_t TWidth, typename TLess>
template<bool TFwd>
skiplist<TValue, TWidth, TLess>::iterator<TFwd>::iterator(skiplist *list, node_ptr node) noexcept :
        list_(list),
        node_(node),
        end_(node)
{ }

template<typename TValue, uint16_t TWidth, typename TLess>
template<bool TFwd>
skiplist<TValue, TWidth, TLess>::iterator<TFwd>::iterator() noexcept :
        list_(nullptr),
        node_(nullptr),
        end_(true)
{ }

template<typename TValue, uint16_t TWidth, typename TLess>
template<bool TFwd>
skiplist<TValue, TWidth, TLess>::iterator<TFwd> &skiplist<TValue, TWidth, TLess>::iterator<TFwd>::operator++() noexcept
{
    if (!node_)
    {
        if (!list_ || end_)
            return *this;

        if (TFwd)
            node_ = list_->front();
        else
            node_ = list_->back();
        return *this;
    }

    auto node = node_;
    if (TFwd)
        node_ = node ? node->next() : nullptr;
    else
        node_ = node ? node->previous() : nullptr;

    if (!node_)
        end_ = true;

    return *this;
}

template<typename TValue, uint16_t TWidth, typename TLess>
template<bool TFwd>
skiplist<TValue, TWidth, TLess>::iterator<TFwd> &skiplist<TValue, TWidth, TLess>::iterator<TFwd>::operator--() noexcept
{
    if (!node_)
    {
        if (!list_ || !end_)
            return *this;

        if (TFwd)
            node_ = list_->back();
        else
            node_ = list_->front();

        end_ = node_;
        return *this;
    }

    auto node = node_;
    if (TFwd)
        node_ = node ? node->previous() : nullptr;
    else
        node_ = node ? node->next() : nullptr;

    return *this;
}

template<typename TValue, uint16_t TWidth, typename TLess>
template<bool TFwd>
bool skiplist<TValue, TWidth, TLess>::iterator<TFwd>::operator==(const iterator &other) const noexcept
{
    return node_ == other.node_;
};

template<typename TValue, uint16_t TWidth, typename TLess>
template<bool TFwd>
bool skiplist<TValue, TWidth, TLess>::iterator<TFwd>::operator!=(const iterator &other) const noexcept
{
    return node_ != other.node_;
}

template<typename TValue, uint16_t TWidth, typename TLess>
template<bool TFwd>
const TValue *skiplist<TValue, TWidth, TLess>::iterator<TFwd>::operator->() const noexcept
{
    if (!node_)
        return nullptr;
    return &node_->value;
}

template<typename TValue, uint16_t TWidth, typename TLess>
template<bool TFwd>
const TValue &skiplist<TValue, TWidth, TLess>::iterator<TFwd>::operator*() const noexcept
{
    return node_->value;
}

template<typename TValue, uint16_t TWidth, typename TLess>
std::default_random_engine skiplist<TValue, TWidth, TLess>::rnd_;

template<typename TValue, uint16_t TWidth, typename TLess>
void skiplist<TValue, TWidth, TLess>::insert(const TValue &value) noexcept
{
    int level = random_level();

    // case1 - the list is empty
    node_ptr head = this->head_;
    if (head == nullptr)
    {
        node_ptr nnode = this->create_node(level, value);
        if (this->head_.compare_exchange_strong(head, nnode))
        {
            this->tail_.compare_exchange_strong(head, nnode);
            return;
        }
    }

    node_ptr needslevels;
    std::array<node_ptr, TWidth> saved;

    // atomically try to insert the node
    node_ptr nnode = this->create_node(level, value);
    while (true)
    {
        // insert the value on level 0
        std::pair<node_ptr, node_ptr> insertloc = this->find_insert_loc(value, saved);

        // case 2: the node already exists in the collection
        if (insertloc.second && insertloc.second->value == value)
        {
            insertloc.second->value = value;

            // there's nothing we need to do other than copying the value
            // no levels or any of that
            return;
        }

        // case 3: value is the new head
        if (!insertloc.first)
        {
            nnode->next[0] = head;
            nnode->previous = nullptr;
            if (this->head_.compare_exchange_strong(head, nnode))
            {
                // we just took over as the head
                // so the node that needs its levels
                // set is the old head
                this->set_previous(nnode, head);

                needslevels = head;
                level = head->level;

                for (int i = 0; i < TWidth; i++)
                    saved[i] = nnode;

                break;
            }

            // the head changed out from under us, we need to re-resolve the location
            continue;
        }

        // case 4: inserting the value after an existing value
        // get the latest values
        nnode->next[0] = insertloc.second;
        nnode->previous = insertloc.first;
        if (insertloc.first->next[0].compare_exchange_strong(insertloc.second, nnode))
        {
            // we successfully inserted it in between first and second.
            // atomically resolve the previous
            this->set_previous(nnode, insertloc.second);

            // and atomically set the tail if necessary
            this->set_tail_if_last(nnode);

            needslevels = std::move(nnode);
            break;
        }

        return;
    }

    // Thus far, we've inserted the node at level 0. Now we have to insert it in all of the appropriate levels
    // It will already be findable and removable after this
    for (int i = 1; i <= level; i++)
    {
        // make sure we have the previous next
        while (true)
        {
            node_ptr pnext = this->scan_values(i, saved[i], value);
            if (pnext == needslevels)
                break;

            // this node effectively doesn't exist in the level noted yet
            // so it's not going to have the next set in this level yet
            // so we can safely do this unatomically
            needslevels->next[i] = pnext;

            if (saved[i]->next[i].compare_exchange_strong(pnext, needslevels))
                break;
        }
    }
}

}

}

#endif //CXXMETRICS_SKIPLIST_HPP_HPP
