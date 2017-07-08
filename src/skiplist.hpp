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
        std::atomic<uint16_t> level;
        node_ptr previous;
        std::array<node_ptr, TWidth> next;
        TValue value;

        node() noexcept :
                level(0)
        {
            static_assert(TWidth < 16384, "Width can't be larger than 0x3fff");
        }

        node(const node &n) :
                value(n.value)
        {
            // we don't copy anything else. The list will take care of that
        }

        uint16_t get_level() const
        {
            return level.load() & 0x3fff;
        }

        bool is_marked() const
        {
            return level.load() >> 15;
        }

        bool is_delete_placeholder()
        {
            return (level.load() >> 14) & 1;
        }
    };

    TLess cmp_;
    node_ptr head_;
    node_ptr tail_;
    pool<node> node_pool;

    _skiplist_data() {}

    _skiplist_data(_skiplist_data &&other)
    {
        auto ptail = other.tail_;
        auto otherhead = other.head_.exchange(nullptr);
        other.tail_ = nullptr;
        other.set_tail_if_last(nullptr);

        head_ = std::move(otherhead);
        set_tail_if_last(ptail);
    }

    ~_skiplist_data()
    {
        clear_all();
    }

    node_ptr create_node(int16_t level, const TValue &value) noexcept
    {
        auto ptr = node_pool.allocate();
        ptr->value = value;
        ptr->level = level;

        return std::move(ptr);
    }

    node_ptr create_delete_placeholder(const node_ptr &replacing) noexcept
    {
        auto ptr = node_pool.allocate();
        ptr->level = 0x4000;
        ptr->previous = replacing->previous;

        return std::move(ptr);
    }

    node_ptr next(int level, const node_ptr &node) noexcept
    {
        node_ptr result = node->next[level];
        while (result && result->is_delete_placeholder())
            result = result->next[level];

        return std::move(result);
    }

    node_ptr scan_next(int level, node_ptr &node) noexcept
    {
        node_ptr node2 = next(level, node);

        while (node2 && (node2->is_marked()))
        {
            node = help_delete(level, node2);
            assert(node);
            node2 = next(level, node);
        }

        assert(!node2 || node2->value > node->value);
        return std::move(node2);
    }

    node_ptr scan_values(int level, node_ptr &less, const TValue &value)
    {
        // back up to a valid starting point
        while (less && (less->is_delete_placeholder() || less->is_marked()))
            less = less->previous;

        if (!less)
            less = front();

        auto more = scan_next(level, less);
        while (more && cmp_(more->value, value))
        {
            less = more;
            more = scan_next(level, less);
        }

        return std::move(more);
    }

    node_ptr scan_last(int level, const node_ptr &hint)
    {
        auto last = hint;
        auto more = scan_next(level, last);
        while (more)
        {
            last = more;
            more = scan_next(level, last);
        }

        return std::move(last);
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

    node_ptr find_loc(const TValue &value)
    {
        node_ptr more = nullptr;
        if (!cmp_(head_->value, value))
            more = front();
        else
        {
            node_ptr less = head_;
            for (int i = TWidth - 1; i >= 0; i--)
                more = scan_values(i, less, value);
            assert(!more || less->value < more->value);
        }

        if (more && more->value == value)
            return std::move(more);
        return nullptr;
    }

    void clear_all() noexcept
    {
        auto cur = head_.exchange(nullptr);
        while (cur)
        {
            // allow 'gc' to happen on our nodes
            cur->previous = nullptr;
            cur = cur->next[0];
        }

        set_tail_if_last();
    }

    node_ptr front() noexcept
    {
        node_ptr f = head_;
        while (f && (f->is_delete_placeholder() || f->is_marked()))
            f = scan_next(0, f);

        return f;
    }

    node_ptr back() noexcept
    {
        auto cur = tail_;
        while (cur && (cur->is_delete_placeholder() || cur->is_marked()))
            cur = previous(0, cur);

        if (!cur)
            return nullptr;

        while (true)
        {
            auto nxt = scan_next(0, cur);
            while (nxt && (nxt->is_delete_placeholder() || nxt->is_marked()))
                nxt = scan_next(0, nxt);

            if (!nxt)
                return std::move(cur);

            cur = nxt;
        }

        return nullptr;
    }

    node_ptr previous(int level, const node_ptr &node) noexcept
    {
        auto prev = node->previous;
        while (prev && ((prev->get_level() < level) || prev->is_marked() || prev->is_delete_placeholder()))
            prev = prev->previous;

        return prev;
    }

    node_ptr scan_previous(int level, const node_ptr &node) noexcept
    {
        auto prev = previous(level, node);
        if (!prev)
            prev = front();

        if (!prev || !cmp_(prev->value, node->value))
            return nullptr;

        for (int i = TWidth - 1; i >= level; i--)
        {
            auto n = next(level, prev);
            while (n && (n->is_marked() || n->get_level() < level))
                n = next(level, n);

            while (n && cmp_(n->value, node->value))
            {
                prev = n;
                n = next(level, n);
                while (n && (n->is_marked() || n->get_level() < level))
                    n = next(level, n);
            }
        }

        return prev;
    }

    node_ptr help_delete(int level, const node_ptr &delnode) noexcept
    {
        // the key element here is that we can't mutate node's next and previous pointers
        // in a way that makes them invalid. We want the node to stay valid as long as. The
        // node will be inaccessible from either side once it's all done. And when others stop
        // using it, it'll lose all of it's references
        node_ptr prev;
        node_ptr after;
        node_ptr node = delnode;

        int backoff = 1;
        while (true)
        {
            prev = scan_previous(level, node);
            after = node->next[level];
            if (!after || !after->is_delete_placeholder())
            {
                this->backoff();
                continue;
            }

            after = after->next[level];

            if (!prev)
            {
                // special case, we have no nodes that will come before us
                // we must have been the head.
                if (!level && head_.compare_exchange_strong(node, after))
                {
                    // we may have lost some next pointers
                    // we need to set the ones that aren't pointing to
                    // the new node
                    for (int i = TWidth - 1; i > 0; i--)
                    {
                        node_ptr expected = next(i, after);
                        while (true)
                        {
                            node_ptr actual = next(i, node);
                            if (actual == after)
                                break;

                            // someone may have inserted something after the new head
                            if (after->next[i].compare_exchange_strong(expected, actual))
                                break;

                            this->backoff(++backoff);
                            continue;
                        }
                    }
                    return std::move(after);
                }

                break;
            }

            if (prev->is_marked())
                continue; // our previous value got marked

            node_ptr expected = node;
            if ((prev->next[level].compare_exchange_strong(expected, after)))
                break;

            if (expected && cmp_(node->value, expected->value))
            {
                after = expected;
                break; // our previous' next has already been set appropriately
            }

            this->backoff(++backoff);
        }

        if (after && (after != prev))
            this->set_previous(prev, after);

        if (!level)
            this->set_tail_if_last();

        return std::move(prev);
    }

    static void backoff(int factor = 1)
    {
        // backoff more as the contention grows
        for (int i = 0; i < factor; i++)
            std::this_thread::yield();
    }

    void set_previous(const node_ptr &hint, node_ptr &node) noexcept
    {
        auto pprev = node->previous;
        auto nprev = hint;

        while (nprev && ((nprev->get_level() < node->get_level()) || nprev->is_marked()))
            nprev = previous(node->get_level(), nprev);

        assert(!nprev || nprev->get_level() >= node->get_level());

        while (true)
        {
            if ((!nprev || (scan_values(node->get_level(), nprev, node->value) == node)) &&
                (node->previous.compare_exchange_strong(pprev, nprev)))
                return;

            backoff();
        }
    }

    void set_tail_if_last() noexcept
    {
        auto tail_at_scan = tail_;
        auto tail = tail_at_scan;

        while (true)
        {
            while (tail && (tail->is_marked() || tail->is_delete_placeholder()))
                tail = scan_previous(0, tail);
            tail = scan_last(0, tail);

            // set the tail to our new tail if it hasn't changed
            if (tail != tail_at_scan)
            {
                if (tail_.compare_exchange_strong(tail_at_scan, tail))
                    break;

                tail = tail_at_scan;
            }
            else
                break;

            this->backoff();
        }
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

        iterator(skiplist *list, const node_ptr &node) noexcept;

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
     * \brief Clear out all entries in the skiplist
     */
    void clear() noexcept;

    /**
     * \brief Find an element by value
     *
     * This is non-const because all calls potentially mutate the internal structures in the list if they are assisting in a delete
     *
     * \param value the value to find in the list
     *
     * \return An iterator to the list location where the value that's being deleted is
     */
    forward_iterator find(const TValue &value) noexcept;

    /**
     * \brief Erase an element from the skiplist
     *
     * \param value The value to erase
     *
     * @return Whether or not the value was erased by this call
     */
    template<bool TFwd>
    bool erase(const iterator<TFwd> &value) noexcept;

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

#ifdef CXXMETRICS_DEBUG
    void dump_nodes(int level) ;
#endif

protected:
    friend class iterator<true>;
    friend class iterator<false>;
};

template<typename TValue, uint16_t TWidth, typename TLess>
template<bool TFwd>
skiplist<TValue, TWidth, TLess>::iterator<TFwd>::iterator(skiplist *list, const node_ptr &node) noexcept :
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
        node_ = node ? list_->scan_next(0, node) : nullptr;
    else
        node_ = node ? list_->scan_previous(0, node) : nullptr;

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
        node_ = node ? list_->scan_previous(0, node) : nullptr;
    else
        node_ = node ? list_->scan_next(0, node) : nullptr;

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
std::default_random_engine skiplist<TValue, TWidth, TLess>::rnd_(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count());

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
        skiplist(other.rbegin(), other.rend())
{ }

template<typename TValue, uint16_t TWidth, typename TLess>
skiplist<TValue, TWidth, TLess>::skiplist(skiplist &&other) noexcept :
        ::cxxmetrics::internal::_skiplist_data<TValue, TWidth, TLess>(std::move(other))
{ }

template<typename TValue, uint16_t TWidth, typename TLess>
skiplist<TValue, TWidth, TLess> &skiplist<TValue, TWidth, TLess>::operator=(const skiplist &other) noexcept
{
    clear();

    for (auto v = other.rbegin(); v != other.rend(); ++v)
        insert(*v);

    return *this;
}

template<typename TValue, uint16_t TWidth, typename TLess>
skiplist<TValue, TWidth, TLess> &skiplist<TValue, TWidth, TLess>::operator=(skiplist &&other) noexcept
{
    auto ptail = other.tail_;
    auto otherhead = other.head_.exchange(nullptr);
    other.tail_ = nullptr;
    other.set_tail_if_last(nullptr);

    while (true)
    {
        clear();
        node_ptr expected = nullptr;
        if (this->head_.compare_exchange_strong(expected, otherhead))
            break;

        // someone stuck a head in clear and try again
        this->backoff();
    }

    set_tail_if_last(ptail);
    return *this;
}

template<typename TValue, uint16_t TWidth, typename TLess>
void skiplist<TValue, TWidth, TLess>::insert(const TValue &value) noexcept
{
    int level = random_level();

    int contention = 1;

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

        this->backoff(contention++);
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
            if (insertloc.second)
            {
                int level = insertloc.second->get_level();
                for (int i = 0; i <= level; i++)
                    nnode->next[i] = insertloc.second;

                for (int i = level + 1; i < TWidth; i++)
                    nnode->next[i] = insertloc.second->next[i];
            }

            nnode->previous = nullptr;
            if (this->head_.compare_exchange_strong(head, nnode))
            {
                // we successfully took over the head
                // so the node that needs its levels
                // set is the old head

                // we have a special case now. Our head has no next pointers.
                // So we need to steal them away from the prior head
                level = head->get_level();

                // let's also start nulling out the old head's nodes
                for (int i = level + 1; i < TWidth; i++)
                {
                    // this is to clean stuff up. It's best effort.
                    // it won't functionally break anything if it's left behind
                    node_ptr n = nnode->next[i];
                    head->next[i].compare_exchange_strong(n, nullptr);
                }

                // set our previous values
                for (int i = 0; i < level; i++)
                {
                    node_ptr newnext = head->next[i];

                    if (newnext && (newnext->get_level() <= i))
                        this->set_previous(head, newnext);
                }
                return;
            }

            // the head changed out from under us, we need to re-resolve the location
            this->backoff(++contention);
            continue;
        }

        assert(!insertloc.second || (!insertloc.second->is_marked() && !insertloc.second->is_delete_placeholder()));

        // case 4: inserting the value after an existing value
        // get the latest values
        nnode->next[0] = insertloc.second;
        nnode->previous = insertloc.first;
        if (nnode->previous->get_level() < level)
            nnode->previous = this->previous(level, nnode->previous);

        if (insertloc.first->next[0].compare_exchange_strong(insertloc.second, nnode))
        {
            // we successfully inserted it in between first and second.
            needslevels = std::move(nnode);
            break;
        }

        this->backoff(++contention);
    }

    // Thus far, we've inserted the node at level 0. Now we have to insert it in all of the appropriate levels
    // It will already be findable and removable after this
    for (int i = 1; i <= level; i++)
    {
        // make sure we have the previous next
        while (true)
        {
            // make sure our saved is the up to date prev
            node_ptr pnext = this->scan_values(i, saved[i], value);

            // only one other thread could be setting this: if the node is being deleted
            node_ptr en;
            needslevels->next[i].compare_exchange_strong(en, pnext);

            // try again if our saved node is marked for deletion
            if (saved[i]->is_marked())
                continue;

            if (pnext == needslevels)
                break; // someone beat us to it

            if (saved[i]->next[i].compare_exchange_strong(pnext, needslevels))
                break;

            needslevels->next[i].compare_exchange_strong(pnext, nullptr);
            this->backoff();
        }
    }

    // until we were locked in, the previous may not have been updated
    // as nodes were inserted.
    this->set_previous(this->scan_previous(level, needslevels), needslevels);

    // atomically resolve the previous
    for (int i = 0; i < level; i++)
    {
        node_ptr newnext = needslevels->next[i];
        if (newnext && (newnext->get_level() <= i))
            this->set_previous(needslevels, newnext);
    }

    // and atomically set the tail if necessary
    this->set_tail_if_last();
}

template<typename TValue, uint16_t TWidth, typename TLess>
typename skiplist<TValue, TWidth, TLess>::forward_iterator skiplist<TValue, TWidth, TLess>::find(const TValue &value) noexcept
{
    return forward_iterator(this, this->find_loc(value));
}

template<typename TValue, uint16_t TWidth, typename TLess>
template<bool TFwd>
bool skiplist<TValue, TWidth, TLess>::erase(const iterator<TFwd> &value) noexcept
{
    node_ptr node = value.node_;
    if (value.list_ != this || !node)
        return false;

    uint16_t level = node->get_level();

    if (level & 0xc000U)
        return false;

    uint16_t newlevel = level | 0x8000U;

    // this is the only mutation the level ever undergoes. If level changed out
    // from under us, it's because the node was deleted
    if (!node->level.compare_exchange_strong(level, newlevel))
        return false;

    // We've marked the node as deleting. Now we have to ensure that nothing gets
    // inserted after it. Anything that looks at the node again will check the level
    // but someone else may have checked it. If they are going to change it, they'll
    // atomically be setting the next values. Which we'll clobber now with a deletion
    // placeholder. So their atomic sets will fail and they'll re-evaluate the node
    auto placeholder = this->create_delete_placeholder(node);
    for (int i = 0; i <= level; i++)
    {
        node_ptr levelnext = node->next[i];
        while (true)
        {
            if (levelnext && levelnext->is_delete_placeholder())
                break;

            placeholder->next[i] = levelnext;
            if (node->next[i].compare_exchange_strong(levelnext, placeholder))
                break;

            this->backoff();
        }
    }

    // and last - we'll just run the delete helper to make sure the node is removed
    // at all levels. It might have already been done
    for (int i = level; i >= 0; i--)
        this->help_delete(i, node);
}

template<typename TValue, uint16_t TWidth, typename TLess>
void skiplist<TValue, TWidth, TLess>::clear() noexcept
{
    this->clear_all();
};

template<typename TValue, uint16_t TWidth, typename TLess>
typename skiplist<TValue, TWidth, TLess>::forward_iterator skiplist<TValue, TWidth, TLess>::begin() noexcept
{
    return forward_iterator(this, this->front());
}

template<typename TValue, uint16_t TWidth, typename TLess>
typename skiplist<TValue, TWidth, TLess>::reverse_iterator skiplist<TValue, TWidth, TLess>::rbegin() noexcept
{
    return reverse_iterator(this, this->back());
}

template<typename TValue, uint16_t TWidth, typename TLess>
typename skiplist<TValue, TWidth, TLess>::forward_iterator skiplist<TValue, TWidth, TLess>::end() noexcept
{
    return forward_iterator(this, nullptr);
}

template<typename TValue, uint16_t TWidth, typename TLess>
typename skiplist<TValue, TWidth, TLess>::reverse_iterator skiplist<TValue, TWidth, TLess>::rend() noexcept
{
    return reverse_iterator(this, nullptr);
}

#ifdef CXXMETRICS_DEBUG
template<typename TValue, uint16_t TWidth, typename TLess>
void skiplist<TValue, TWidth, TLess>::dump_nodes(int level)
{
    node_ptr cur = this->head_;
    if (!cur)
    {
        std::cout << "Level " << level << " -- (null)" << std::endl;
        return;
    }

    assert(!cur->is_delete_placeholder());

    std::cout << "Level " << level << " -- ";
    if (cur->previous)
    {
        std::cout << "(prev: " << cur->previous->value << ") ";
    }

    std::cout << cur->value;
    if (cur->is_marked())
        std::cout << "(tagged_deleted)";

    auto prev = cur;
    while (cur->next[level])
    {
        cur = cur->next[level];

        if (cur->is_delete_placeholder())
        {
            std::cout << " -> <del placeholder>";
            continue;
        }

        std::cout << " -> ";
        if (cur->previous)
        {
            if (cur->previous->next[cur->get_level()] != cur)
                std::cout << "(different prev: " << cur->previous->value << ") ";
        }
        std::cout << "[L" << cur->get_level() << "] " << cur->value;
        if (cur->is_marked())
            std::cout << "(tagged_deleted)";

        prev = cur;
    }

    std::cout << std::endl;
}
#endif

}

}

#endif //CXXMETRICS_SKIPLIST_HPP_HPP
