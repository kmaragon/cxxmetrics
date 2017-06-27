#ifndef CXXMETRICS_SKIPLIST_HPP
#define CXXMETRICS_SKIPLIST_HPP

#include <random>
#include <atomic>
#include <thread>
#include "pool.hpp"

namespace cxxmetrics
{

namespace internal
{

template<typename TValue, uint16_t TWidth = 8, typename TLess = std::less<TValue>>
class _skiplist_node_container
{
public:

    class node;
    using node_ptr = pool_ptr<node>;
    using atomic_node = std::atomic<node_ptr>;

    struct node
    {
        std::atomic_uint_fast16_t level;
        uint16_t valid;
        std::atomic_int_fast32_t refcount;

        TValue value;
        atomic_node previous;
        std::array<atomic_node, TWidth> next;

        node() noexcept :
                level(0),
                valid(0),
                refcount(1)
        {
            static_assert(TWidth < 32768, "Width can't be larger than 0x7fff");
        }

        bool is_marked() const
        {
            return level.load() >> 15;
        }
    };

    // members
    TLess cmp_;

    atomic_node head_;
    atomic_node tail_;
    pool<node> node_pool;

    _skiplist_node_container() noexcept
    { }

    _skiplist_node_container(_skiplist_node_container &&other) noexcept :
            head_(other.head_),
            tail_(other.tail_)
    {
        other.head_ = nullptr;
        other.tail_ = nullptr;
    }

    ~_skiplist_node_container()
    {
        node_ptr cur = this->head_;
        while (cur)
        {
            node_ptr tmp = cur;
            cur = cur->next[0];
            node_pool.hard_delete(tmp);
        }
    }

    inline node_ptr unmarked(node_ptr ref) noexcept
    {
        return node_ptr(reinterpret_cast<::cxxmetrics::internal::_pool_data<node> *>(reinterpret_cast<unsigned long>(ref.ptr()) & ~((unsigned long)1)));
    }

    inline node_ptr marked(node_ptr ref) noexcept
    {
        if (!unmarked(ref))
            return nullptr;
        return node_ptr(reinterpret_cast<::cxxmetrics::internal::_pool_data<node> *>(reinterpret_cast<unsigned long>(ref.ptr()) | 1));
    }

    bool is_marked(node_ptr ref) noexcept
    {
        return reinterpret_cast<unsigned long>(ref.ptr()) & 1;
    }

    node_ptr add_ref(node_ptr ref) noexcept
    {
        if (is_marked(ref))
            return nullptr;

        ref = unmarked(ref);
        if (!ref)
            return nullptr;

        if (ref->refcount.fetch_add(1) == 0)
        {
            ref->refcount -= 1;
            return nullptr;
        }

        return ref;
    }

    node_ptr remove_ref(node_ptr ref) noexcept
    {
        ref = unmarked(ref);
        if (ref->refcount.fetch_sub(1) == 1)
            node_pool.finish(ref);

        return ref;
    }

    node_ptr get_previous(node_ptr node) noexcept
    {
        auto &value = node->value;
        auto previous = add_ref(node->previous.load());
        if (!previous)
            previous = add_ref(head_.load());

        auto node2 = this->scan_value(previous, 0, value);
        if (node2)
            remove_ref(node2);
        return previous;
    }

    node_ptr next(node_ptr node, int level) noexcept
    {
        if (node->is_marked())
            node = help_delete(node, level);

        if (!node)
            return nullptr;

        auto node2 = node->next[level].load();
        if (!node2)
            return nullptr; // there is no next

        node2 = add_ref(node2);
        while (!node2)
        {
            node = help_delete(node, level);
            if (node)
                node2 = add_ref(node->next[level]);
            else
                node2 = nullptr;
        }
        return node2;
    }

    node_ptr scan_value(node_ptr &node, int level, const TValue &value) noexcept
    {
        if (!node)
            node = add_ref(head_);

        node_ptr node2;
        if (!node || !cmp_(node->value, value))
        {
            node2 = node;
            node = nullptr;
            return node2;
        }

        node2 = next(node, level);
        while (node2 && cmp_(node2->value, value))
        {
            remove_ref(node);
            node = node2;
            node2 = next(node, level);
        }

        return node2;
    }

    unique_pool_ptr<node> create_node(int level, const TValue &value) noexcept
    {
        auto ptr = node_pool.get();
        ptr->refcount = 1;
        ptr->level = level;
        ptr->value = value;

        for (int i = 0; i < TWidth; i++)
            ptr->next[i] = nullptr;

        return std::move(ptr);
    }

    node_ptr help_delete(node_ptr node, int level) noexcept
    {
        if (!node)
            return nullptr;

        // mark all of the next pointers as being in-delete
        for (int i = level; i < static_cast<int16_t>(node->level.load() & 0x7fff) - 1; i++)
        {
            node_ptr node2;
            uint16_t level2;
            do
            {
                node2 = node->next[i].load();
            }
            while (node2 && !is_marked(node2) && !node->next[i].compare_exchange_strong(node2, marked(node2)));
        }

        node_ptr prev;

        // loop to remove the node at the level
        while (true)
        {
            // get the previous value
            prev = get_previous(node);

            // there's no next at this level anyway
            if (!node->next[level].load())
                break;

            // remove the node from this level - if it works, then we did our job
            node_ptr next = node;
            if (prev->next[level].compare_exchange_strong(next, unmarked(node->next[level].load())))
            {
                node->next[level] = nullptr;
                remove_ref(prev);
                break;
            }

            remove_ref(prev);
            std::this_thread::yield();
        }

        // we're done with the node
        remove_ref(node);
        return prev;
    }
};

/**
 * \brief An implementation of 'Fast and Lock-Free Concurrent Priority Queues for Multi-Threaded Systems' by Sundell and Tsigas
 *
 * This is a special skiplist implementation that operates lock free by fairly precisely implementing the noted paper
 *
 * \tparam TValue the type of value in the skiplist
 * \tparam TWidth the width of the skipwidth. This will have a dramatic impact on memory usage
 * \tparam TLess the comparator to use for sorting the skiplist
 */
template<typename TValue, uint16_t TWidth = 8, typename TLess = std::less<TValue>>
class skiplist : protected internal::_skiplist_node_container<TValue, TWidth, TLess>
{
    using node_ptr = typename internal::_skiplist_node_container<TValue, TWidth, TLess>::node_ptr;
    using atomic_node = typename internal::_skiplist_node_container<TValue, TWidth, TLess>::atomic_node;
    int random_level()
    {
        std::uniform_int_distribution<int> dst(0, TWidth - 1);
        return dst(rnd_);
    }

    std::default_random_engine rnd_;

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
        iterator(const iterator &) noexcept;
        ~iterator();
        iterator &operator=(const iterator &other) noexcept = default;

        iterator &operator++() noexcept;
        iterator &operator--() noexcept;

        bool operator==(const iterator &other) const noexcept;
        bool operator!=(const iterator &other) const noexcept;

        const TValue *operator->() const noexcept;
        const TValue &operator*() const noexcept;

    };

    using forward_iterator = iterator<true>;
    using reverse_iterator = iterator<false>;

    /**
     * \brief Default constructor
     */
    skiplist() noexcept;

    /**
     * \brief Construct A skiplist and fill it with the specified data
     *
     * \tparam TInputIterator The type of iterator that supplies the data
     *
     * \param begin The begin iterator
     * \param end the end iterator
     */
    template<typename TInputIterator>
    skiplist(TInputIterator begin, const TInputIterator &end) noexcept;

    /**
     * \brief Copy constructor
     */
    skiplist(const skiplist &other) noexcept;

    /**
     * \brief Move constructor
     */
    skiplist(skiplist &&other) noexcept;

    ~skiplist() = default;

    /**
     * \brief Assignment Operator
     */
    skiplist &operator=(const skiplist &other) noexcept;

    /**
     * \brief Move Assignment operator
     */
    skiplist &operator=(skiplist &&other) noexcept;

    /**
     * \brief Insert an item into the skiplist
     *
     * \param value the value to insert into the list
     */
    void insert(const TValue &value) noexcept;

    /**
     * \brief Get a forward iterator into the skiplist
     *
     * \return a forward iterator into the skiplist
     */
    forward_iterator begin() noexcept;

    /**
     * \brief Get a reverse iterator into the skiplist
     *
     * \return a reverse iterator into the skiplist
     */
    reverse_iterator rbegin() noexcept;

    /**
     * \brief Get the end of the list for a forward iterator
     *
     * \return the end of the list
     */
    forward_iterator end() noexcept;

    /**
     * \brief Get the end of the list for a reverse iterator
     *
     * \return the end of the list
     */
    reverse_iterator rend() noexcept;

private:
    friend class iterator<true>;
    friend class iterator<false>;
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
skiplist<TValue, TWidth, TLess>::iterator<TFwd>::iterator(const iterator &other) noexcept :
        list_(other.list_),
        node_(other.list_ ? other.list_->add_ref(other.node_) : nullptr),
        end_(other.node_)
{ }

template<typename TValue, uint16_t TWidth, typename TLess>
template<bool TFwd>
skiplist<TValue, TWidth, TLess>::iterator<TFwd>::~iterator()
{
    if (node_ && list_)
        list_->remove_ref(node_);
}

template<typename TValue, uint16_t TWidth, typename TLess>
template<bool TFwd>
skiplist<TValue, TWidth, TLess>::iterator<TFwd> &skiplist<TValue, TWidth, TLess>::iterator<TFwd>::operator++() noexcept
{
    if (!node_)
    {
        if (!list_ || end_)
            return *this;

        if (TFwd)
            node_ = list_->add_ref(list_->head_);
        else
            node_ = list_->add_ref(list_->tail_);
        return *this;
    }

    auto node = node_;
    if (TFwd)
        node_ = list_->add_ref(node_->next[0]);
    else
        node_ = list_->get_previous(node_);
    list_->remove_ref(node);

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
            node_ = list_->add_ref(list_->tail_);
        else
            node_ = list_->add_ref(list_->head_);

        end_ = node_;
        return *this;
    }

    auto node = node_;
    if (TFwd)
        node_ = list_->get_previous(node_);
    else
        node_ = list_->add_ref(node_->next[0]);

    if (!node_)
    {
        node_ = node;
        return *this;
    }

    list_->remove_ref(node);
    return *this;
}

template<typename TValue, uint16_t TWidth, typename TLess>
template<bool TFwd>
bool skiplist<TValue, TWidth, TLess>::iterator<TFwd>::operator==(const iterator &other) const noexcept
{
    return node_.ptr() == other.node_.ptr();
};

template<typename TValue, uint16_t TWidth, typename TLess>
template<bool TFwd>
bool skiplist<TValue, TWidth, TLess>::iterator<TFwd>::operator!=(const iterator &other) const noexcept
{
    return node_.ptr() != other.node_.ptr();
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
skiplist<TValue, TWidth, TLess>::skiplist() noexcept
{ }

template<typename TValue, uint16_t TWidth, typename TLess>
template<typename TInputIterator>
skiplist<TValue, TWidth, TLess>::skiplist(TInputIterator begin, const TInputIterator &end) noexcept
{
    for (; begin != end; ++begin)
        insert(*begin);
}

template<typename TValue, uint16_t TWidth, typename TLess>
skiplist<TValue, TWidth, TLess>::skiplist(const skiplist &other) noexcept :
        skiplist(rbegin(), rend())
{ }

template<typename TValue, uint16_t TWidth, typename TLess>
skiplist<TValue, TWidth, TLess>::skiplist(skiplist &&other) noexcept :
        ::cxxmetrics::internal::_skiplist_node_container<TValue, TWidth, TLess>(std::move(other))
{ }

template<typename TValue, uint16_t TWidth, typename TLess>
skiplist<TValue, TWidth, TLess> &skiplist<TValue, TWidth, TLess>::operator=(const skiplist &other) noexcept
{
    auto head = this->head_;

    while (true)
    {
        if (this->head_.compare_exchange_strong(head, nullptr))
        {
            this->tail_ = nullptr;

            while (head)
            {
                node_ptr tmp = head;
                head = head->next[0];
                this->remove_ref(tmp);
            }

            break;
        }

        std::this_thread::yield();
    }

    for (auto v = other.rbegin(); v != other.rend(); ++v)
        insert(*v);

    return *this;
}

template<typename TValue, uint16_t TWidth, typename TLess>
skiplist<TValue, TWidth, TLess> &skiplist<TValue, TWidth, TLess>::operator=(skiplist &&other) noexcept
{
    auto head = this->head_;

    if (this == &other)
    {
        // there are no guarantees about moving from self
        // except that we won't crash. But in this case,
        // we can crash
        return *this;
    }

    bool swapped_other = false;
    node_ptr otherhead = other.head_.load();
    node_ptr othertail;
    while (true)
    {
        if (!swapped_other)
        {
            if (!other.head_.compare_exchange_strong(otherhead, nullptr))
                continue;

            other.tail_.exchange(othertail);
            swapped_other = true;
        }

        if (this->head_.compare_exchange_strong(head, otherhead))
        {
            this->tail_ = othertail;

            // we swapped our head - now clean up our old stuff
            while (head)
            {
                node_ptr tmp = head;
                head = head->next[0];
                this->remove_ref(tmp);
            }

            break;
        }

        std::this_thread::yield();
    }

    return *this;
}

template<typename TValue, uint16_t TWidth, typename TLess>
void skiplist<TValue, TWidth, TLess>::insert(const TValue &value) noexcept
{
    int level = random_level();
    auto newnode = this->add_ref(this->create_node(level, value).release());

    // first node
    auto node = this->add_ref(this->head_.load());
    node_ptr saved[TWidth];

    for (int i = TWidth - 1; i > 0; i--)
    {
        auto node2 = this->scan_value(node, i, value);
        if (node2)
            this->remove_ref(node2);

        if (!node)
            break; // this is the new head... there's no saves

        if (i < level)
            saved[i] = this->add_ref(node);
    }

    node_ptr node2;
    while (true)
    {
        node2 = this->scan_value(node, 0, value);
        if (node2)
        {
            if (!node2->is_marked() && node2->value == value)
            {
                node2->value = value;

                // the element already exists and isn't marked for deletion
                this->remove_ref(node2);

                if (node)
                {
                    this->remove_ref(node);
                    for (int i = 0; i < level - 1; i++)
                    {
                        auto savedat = saved[i];
                        if (savedat)
                            this->remove_ref(savedat);
                    }
                }

                this->remove_ref(newnode);
                this->remove_ref(newnode);
                return;
            }

            this->remove_ref(node2);
        }

        // set the tail
        node_ptr tail = node;

        newnode->previous = node;
        newnode->next[0] = node2;

        if (!node)
        {
            if (this->head_.compare_exchange_strong(node2, newnode))
            {
                // set the tail to newnode if our node was the tail
                this->tail_.compare_exchange_strong(tail, newnode);
                break;
            }

            node = this->add_ref(this->head_.load());
            this->remove_ref(node2);
            continue;
        }

        if (node->next[0].compare_exchange_strong(node2, newnode))
        {
            // set the tail to newnode if our node was the tail
            this->tail_.compare_exchange_strong(tail, newnode);
            this->remove_ref(node);
            break;
        }

        std::this_thread::yield();
    }

    if (node2)
    {
        node_ptr prev = node;
        if (!newnode->is_marked())
            node2->previous.compare_exchange_strong(prev, newnode);
    }

    for (int i = 1; i <= level; i++)
    {
        newnode->valid = i;
        node = saved[i];
        while (node)
        {
            auto node2 = this->scan_value(node, i, value);
            newnode->next[i] = node2;

            if (node2)
                this->remove_ref(node2);

            if (newnode->is_marked() ||
                node->next[i].compare_exchange_strong(node2, newnode))
            {
                this->remove_ref(node);
                break;
            }

            std::this_thread::yield();
        }
    }

    newnode->valid = level;
    if (newnode->is_marked())
        newnode = this->help_delete(newnode, 0);

    this->remove_ref(newnode);
}

template<typename TValue, uint16_t TWidth, typename TLess>
typename skiplist<TValue, TWidth, TLess>::forward_iterator skiplist<TValue, TWidth, TLess>::begin() noexcept
{
    return forward_iterator(this, this->add_ref(this->head_.load()));
}

template<typename TValue, uint16_t TWidth, typename TLess>
typename skiplist<TValue, TWidth, TLess>::reverse_iterator skiplist<TValue, TWidth, TLess>::rbegin() noexcept
{
    return reverse_iterator(this, this->add_ref(this->tail_.load()));
}

template<typename TValue, uint16_t TWidth, typename TLess>
typename skiplist<TValue, TWidth, TLess>::forward_iterator skiplist<TValue, TWidth, TLess>::end() noexcept
{
    return forward_iterator(this, nullptr);
}

template<typename TValue, uint16_t TWidth, typename TLess>
typename skiplist<TValue, TWidth, TLess>::reverse_iterator skiplist<TValue, TWidth, TLess>::rend() noexcept
{
    return reverse_iterator(this, this->add_ref(nullptr));
}

}
}

#endif // CXXMETRICS_SKIPLIST_HPP