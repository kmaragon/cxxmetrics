#ifndef CXXMETRICS_SKIPLIST_HPP_HPP
#define CXXMETRICS_SKIPLIST_HPP_HPP

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
        std::atomic<uint16_t> valid_level;
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
    pool<node> node_pool;

    _skiplist_data() {}

    _skiplist_data(_skiplist_data &&other)
    {
        auto otherhead = other.head_.exchange(nullptr);

        head_ = std::move(otherhead);
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
        ptr->valid_level = 0;

        return std::move(ptr);
    }

    node_ptr create_delete_placeholder(const node_ptr &replacing) noexcept
    {
        auto ptr = node_pool.allocate();
        ptr->level = 0x4000;
        ptr->valid_level = 0;

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
        assert (node2 != node);
        assert (!node2 || node2->is_marked() || node->is_marked() || cmp_(node->value, node2->value));

        TValue pvalue = node->value;
        while (node2 && (node2->is_marked()))
        {
            node = help_delete(level, node, node2);
            if (!node)
                return nullptr;

            assert(!cmp_(pvalue, node->value));
            node2 = next(level, node);
            assert (node2 != node);
        }

        assert (!node2 || node->is_marked() || cmp_(node->value, node2->value));
        return std::move(node2);
    }

    node_ptr scan_values(int level, node_ptr &less, const TValue &value)
    {
        node_ptr more;
        if (!less || !cmp_(less->value, value))
        {
            less = front();
            if (!less)
                return nullptr;
        }

        if (!cmp_(less->value, value))
        {
            more = less;
            less = nullptr;
            return more;
        }

        more = scan_next(level, less);
        while (more && cmp_(more->value, value))
        {
            less = more;
            more = scan_next(level, less);
        }

        assert(!less || cmp_(less->value, value));
        assert(!more || !cmp_(more->value, value));
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
        node_ptr less = head_;
        node_ptr more = nullptr;

        for (int i = TWidth - 1; i >= 0; i--)
        {
            more = scan_values(i, less, value);
            saved[i] = less;
            assert(!less || cmp_(less->value, value));
            assert(!more || !cmp_(more->value, value));
        }

        return std::make_pair(std::move(less), std::move(more));
    }

    node_ptr find_loc(const TValue &value)
    {
        node_ptr more = nullptr;
        node_ptr less = head_;
        for (int i = TWidth - 1; i >= 0; i--)
            more = scan_values(i, less, value);

        assert(!more || !less || less->value < more->value);
        if (more && more->value == value)
            return std::move(more);
        return nullptr;
    }

    void clear_all() noexcept
    {
        auto cur = head_.exchange(nullptr);
    }

    node_ptr front() noexcept
    {
        node_ptr f = head_;
        while (f && f->is_marked())
            f = next(0, f);

        return f;
    }

    node_ptr help_delete(int level, const node_ptr &previous, const node_ptr &delnode) noexcept
    {
        // the key element here is that we can't mutate node's next and previous pointers
        // in a way that makes them invalid. We want the node to stay valid as long as. The
        // node will be inaccessible from either side once it's all done. And when others stop
        // using it, it'll lose all of it's references
        node_ptr after;
        node_ptr prev_replace = previous;
        node_ptr prev = previous;
        node_ptr node = delnode;

        int backoff = 1;
        node_ptr expected = node;

        while (true)
        {
            // what if prev is being deleted?
            // we need to rescan
            if (!prev || prev->is_marked())
            {
                prev_replace = head_;
                if (prev_replace)
                {
                    for (int i = TWidth - 1; i >= level; i--)
                    {
                        scan_values(i, prev_replace, prev_replace->value);
                        if (!prev_replace)
                            break;
                    }
                }

                prev = prev_replace;
                // re-resolve prev
                scan_values(level, prev, node->value);
            }

            after = node->next[level];
            if (!after || !after->is_delete_placeholder())
            {
                this->backoff();
                continue;
            }

            after = next(level, node);
            while (after && after->is_marked())
                after = next(level, after);

            if (!prev)
            {
                // special case, we have no nodes that will come before us
                // we must have been the head.
                if (!level)
                {
                    if (node->valid_level < (TWidth - 1))
                    {
                        // let the head finish becoming head first
                        // ...
                        // unless it was already relieved of head duty
                        if (head_ != node)
                            break;

                        // we may need to re-resolve prev in this case
                        this->backoff();
                        continue;
                    }

                    if (head_.compare_exchange_strong(node, after))
                    {
                        if (after)
                        {
                            for (int i = after->get_level() + 1; i < TWidth; i++)
                            {
                                // these should all essentially be null
                                // they are pointers for levels that after doesn't
                                // know about. So after can't be in them either
                                assert(after != node->next[i]);
                                node_ptr ptr;
                                after->next[i].compare_exchange_strong(ptr, next(i, node));

                                uint16_t vlevel = after->valid_level.load();
                                while (vlevel < i)
                                {
                                    if (after->valid_level.compare_exchange_strong(vlevel, i))
                                        break;
                                }
                            }
                        }

                        return nullptr;
                    }
                }

                break;
            }

            if (after && after->is_marked())
                continue;

            assert(prev != after);
            assert(!after || cmp_(prev->value, after->value));
            if (prev->next[level].compare_exchange_strong(expected, after))
            {
                // the only authorative check for after being marked for deletion
                if (after && after->is_marked())
                {
                    expected = after;
                    continue;
                }

                break;
            }

            if (expected == after)
                break; // our previous' next has already been set appropriately

            if (expected && cmp_(expected->value, node->value))
                prev = expected;
            else
                break;

            expected = node;
            this->backoff(++backoff);
        }

        return std::move(prev_replace);
    }

    static void backoff(int factor = 1)
    {
        // backoff more as the contention grows
        for (int i = 0; i < factor; i++)
            std::this_thread::yield();
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
    class iterator : public std::iterator<std::input_iterator_tag, TValue>
    {
        skiplist *list_;
        node_ptr node_;

        iterator(skiplist *list, const node_ptr &node) noexcept;

        friend class skiplist;
    public:
        iterator() noexcept;
        iterator(const iterator &) noexcept = default;
        ~iterator() = default;
        iterator &operator=(const iterator &other) noexcept = default;

        iterator &operator++() noexcept;

        bool operator==(const iterator &other) const noexcept;
        bool operator!=(const iterator &other) const noexcept;

        const TValue *operator->() const noexcept;
        const TValue &operator*() const noexcept;
    };

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
    bool insert(const TValue &value) noexcept;

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
    iterator find(const TValue &value) noexcept;

    /**
     * \brief Erase an element from the skiplist
     *
     * \param value The value to erase
     *
     * @return Whether or not the value was erased by this call
     */
    bool erase(const iterator &value) noexcept;

    /**
     * \brief Get an iterator into the skiplist
     *
     * \return an iterator into the skiplist
     */
    iterator begin() noexcept;

    /**
     * \brief Get the end of the list for an iterator
     *
     * \return the end of the list
     */
    iterator end() noexcept;

#ifdef CXXMETRICS_DEBUG
    void dump_nodes(int level) ;
#endif

protected:
    friend class iterator;
};

template<typename TValue, uint16_t TWidth, typename TLess>
skiplist<TValue, TWidth, TLess>::iterator::iterator(skiplist *list, const node_ptr &node) noexcept :
        list_(list),
        node_(node)
{ }

template<typename TValue, uint16_t TWidth, typename TLess>
skiplist<TValue, TWidth, TLess>::iterator::iterator() noexcept :
        list_(nullptr),
        node_(nullptr)
{ }

template<typename TValue, uint16_t TWidth, typename TLess>
typename skiplist<TValue, TWidth, TLess>::iterator &skiplist<TValue, TWidth, TLess>::iterator::operator++() noexcept
{
    if (!node_)
        return *this;

    auto node = node_;
    node_ = node ? list_->scan_next(0, node) : nullptr;

    return *this;
}

template<typename TValue, uint16_t TWidth, typename TLess>
bool skiplist<TValue, TWidth, TLess>::iterator::operator==(const iterator &other) const noexcept
{
    return node_ == other.node_;
};

template<typename TValue, uint16_t TWidth, typename TLess>
bool skiplist<TValue, TWidth, TLess>::iterator::operator!=(const iterator &other) const noexcept
{
    return node_ != other.node_;
}

template<typename TValue, uint16_t TWidth, typename TLess>
const TValue *skiplist<TValue, TWidth, TLess>::iterator::operator->() const noexcept
{
    if (!node_)
        return nullptr;
    return &node_->value;
}

template<typename TValue, uint16_t TWidth, typename TLess>
const TValue &skiplist<TValue, TWidth, TLess>::iterator::operator*() const noexcept
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
    auto otherhead = other.head_.exchange(nullptr);

    while (true)
    {
        clear();
        node_ptr expected = nullptr;
        if (this->head_.compare_exchange_strong(expected, otherhead))
            break;

        // someone stuck a head in clear and try again
        this->backoff();
    }

    return *this;
}

template<typename TValue, uint16_t TWidth, typename TLess>
bool skiplist<TValue, TWidth, TLess>::insert(const TValue &value) noexcept
{
    int level = random_level();

    int contention = 1;

    // case1 - the list is empty
    node_ptr head = this->head_;

    node_ptr needslevels;
    std::array<node_ptr, TWidth> saved;

    // atomically try to insert the node
    node_ptr nnode = this->create_node(level, value);
    while (true)
    {
        if (!head)
        {
            nnode->valid_level = TWidth - 1;
            if (this->head_.compare_exchange_strong(head, nnode))
                return true;

            nnode->valid_level = 0;
        }

        // insert the value on level 0
        std::pair<node_ptr, node_ptr> insertloc = this->find_insert_loc(value, saved);

        // case 2: the node already exists in the collection
        if (insertloc.second && insertloc.second->value == value)
        {
            insertloc.second->value = value;

            // there's nothing we need to do other than copying the value
            // no levels or any of that
            return false;
        }

        // case 3: value is the new head
        if (!insertloc.first)
        {
            // set is the old head
            if (insertloc.second)
            {
                int level = insertloc.second->get_level();
                for (int i = 0; i <= level; i++)
                    nnode->next[i] = insertloc.second;

                for (int i = level + 1; i < TWidth; i++)
                    nnode->next[i] = insertloc.second->next[i];
            }
            else if (head)
            {
                // wtf - we have a head but no next and no prev?
                // how does this happen?
                this->backoff();
                continue;
            }

            nnode->next[0] = insertloc.second;

            // we're inserting the value as the new head. Which means it must be smaller
            if (!head->is_marked() && !this->cmp_(value, head->value))
            {
                this->backoff();
                continue; // wtf is happening here?
            }

            if (head->valid_level < TWidth - 1)
            {
                // the head is still in prep
                // ...
                /// unless it's not the head anymore
                head = this->head_;
                this->backoff();
                continue;
            }

            if (this->head_.compare_exchange_strong(head, nnode))
            {
                // we have a special case now. Our head has no next pointers.
                // So we need to steal them away from the prior head
                level = head->get_level();
                uint16_t expectedlevel = TWidth - 1;
                head->valid_level.compare_exchange_strong(expectedlevel, level);

                // let's also start nulling out the old head's nodes
                for (int i = level + 1; i < TWidth; i++)
                {
                    // this is to clean stuff up. It's best effort.
                    // it won't functionally break anything if it's left behind
                    node_ptr n = nnode->next[i];
                    head->next[i].compare_exchange_strong(n, nullptr);
                }

                expectedlevel = 0;
                nnode->valid_level.compare_exchange_strong(expectedlevel, TWidth - 1);
                return true;
            }

            // the head changed out from under us, we need to re-resolve the location
            this->backoff(++contention);
            continue;
        }

        assert(!insertloc.second || !insertloc.second->is_delete_placeholder());

        // case 4: inserting the value after an existing value
        // get the latest values
        nnode->next[0] = insertloc.second;

        assert(insertloc.first != nnode);
        assert(this->cmp_(insertloc.first->value, value));
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
            node_ptr pnext = this->scan_values(i, saved[i], needslevels->value);

            if (pnext == needslevels)
                break; // someone beat us to it

            // only one other thread could be setting this: if the node is being deleted
            node_ptr en;
            assert(!pnext || this->cmp_(needslevels->value, pnext->value));
            if (!needslevels->next[i].compare_exchange_strong(en, pnext))
                break; // the node is getting deleted. Might as well just skip this

            // try again if our saved node is marked for deletion
            if (!saved[i] || saved[i]->is_marked() || !this->cmp_(saved[i]->value, needslevels->value))
            {
                // unfortunately our previous is before the one we had tracked
                // so we'll need to re-scan at this level
                saved[i] = this->head_;

                // rare case, the list became empty while we were inserting
                if (!saved[i])
                    break;

                for (int x = TWidth - 1; x >= i; x--)
                {
                    if (!saved[i])
                        break;
                    this->scan_values(x, saved[i], needslevels->value);
                }

                if (!saved[i])
                    break;

                this->backoff(++contention);
                continue;
            }

            assert(needslevels != saved[i]);
            assert(this->cmp_(saved[i]->value, needslevels->value));
            if (saved[i]->next[i].compare_exchange_strong(pnext, needslevels))
                break;

            needslevels->next[i].compare_exchange_strong(pnext, nullptr);
            this->backoff(++contention);
        }

        uint16_t vlevel = needslevels->valid_level.load();
        while (vlevel < i)
        {
            if (needslevels->valid_level.compare_exchange_strong(vlevel, i))
                break;
        }
    }

    return true;
}

template<typename TValue, uint16_t TWidth, typename TLess>
typename skiplist<TValue, TWidth, TLess>::iterator skiplist<TValue, TWidth, TLess>::find(const TValue &value) noexcept
{
    return iterator(this, this->find_loc(value));
}

template<typename TValue, uint16_t TWidth, typename TLess>
bool skiplist<TValue, TWidth, TLess>::erase(const iterator &value) noexcept
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
    for (int i = 0; i < TWidth; i++)
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

    node_ptr prev = this->head_;
    if (prev == node)
        prev = nullptr;

    for (int i = TWidth - 1; i >= 0; i--)
    {
        // resolve the previous for the level
        if (prev)
            this->scan_values(i, prev, node->value);

        // delete the node
        if (i <= node->get_level())
            this->help_delete(i, prev, node);
    }

    return true;
}

template<typename TValue, uint16_t TWidth, typename TLess>
void skiplist<TValue, TWidth, TLess>::clear() noexcept
{
    this->clear_all();
};

template<typename TValue, uint16_t TWidth, typename TLess>
typename skiplist<TValue, TWidth, TLess>::iterator skiplist<TValue, TWidth, TLess>::begin() noexcept
{
    return iterator(this, this->front());
}

template<typename TValue, uint16_t TWidth, typename TLess>
typename skiplist<TValue, TWidth, TLess>::iterator skiplist<TValue, TWidth, TLess>::end() noexcept
{
    return iterator(this, nullptr);
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

        std::cout << " -> [L" << cur->get_level() << "] " << cur->value;
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
