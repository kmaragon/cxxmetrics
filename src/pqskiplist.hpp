#ifndef CXXMETRICS_PQSKIPLIST_HPP
#define CXXMETRICS_PQSKIPLIST_HPP

#include <atomic>
#include <array>
#include <random>

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
    static constexpr unsigned long DELETE_MARKER = ((unsigned long) 1) << ((sizeof(void *) * 8) - 1);
    static constexpr unsigned int LEVEL_DELETE_MARKER = 1 << ((sizeof(int) * 8) - 1);
    std::array<ptr, width> next_;
    T value_;

    std::atomic_uint_fast64_t refs_;

    std::atomic_uint_least16_t level_;
    std::atomic_uint_least8_t inlist_;

public:
    constexpr static skiplist_node *marked_ptr(skiplist_node *ptr)
    {
        return reinterpret_cast<skiplist_node *>(reinterpret_cast<unsigned long>(ptr) | DELETE_MARKER);
    }

    constexpr static skiplist_node *unmarked_ptr(skiplist_node *ptr)
    {
        return reinterpret_cast<skiplist_node *>(reinterpret_cast<unsigned long>(ptr) & ~DELETE_MARKER);
    }

    constexpr static bool ptr_is_marked(skiplist_node *ptr)
    {
        return (reinterpret_cast<unsigned long>(ptr) & DELETE_MARKER) != 0;
    }

    int level() const noexcept
    {
        return level_.load() & ~LEVEL_DELETE_MARKER;
    }

    bool reference() noexcept
    {
        auto refs = refs_.load();
        while (true)
        {
            if (refs == 0)
                return false;

            if (refs_.compare_exchange_strong(refs, refs + 1))
                return true;
        }
    }

    template<typename TAlloc>
    void dereference(TAlloc &t) noexcept
    {
        if (refs_.fetch_sub(1) == 1)
            t.deallocate(this);
    }

    bool is_marked() const noexcept
    {
        return (level_.load() & LEVEL_DELETE_MARKER) != 0;
    }

    const T &value() const noexcept
    {
        return value_;
    }

    void init(const T &value, int level) noexcept
    {
        refs_ = 1;
        level_ = level;
        value_ = value;
        for (int i = 0; i < width; i++)
            next_[i] = nullptr;
    }

    std::pair<skiplist_node *, bool> next(int level) const noexcept
    {
        auto res = next_[level].load();
        auto ptr = reinterpret_cast<skiplist_node *>(reinterpret_cast<unsigned long>(res) & ~DELETE_MARKER);
        return std::make_pair(ptr, !ptr_is_marked(res));
    }

    // sets the next node as long as the node isn't being deleted
    bool set_next(int level, skiplist_node *node) noexcept
    {
        next_[level] = node;
        return !node->is_marked();
    }

    // inserts the given node as the next node after this
    bool insert_next(int level, skiplist_node *next, skiplist_node *node) noexcept
    {
        node->next_[level] = next;
        next_[level].compare_exchange_strong(next, node);
    }

    // replace the next node with newnext if expected is the next at this level
    // requires that the next node has already been marked for deletion
    bool remove_next(int level, skiplist_node *expected, skiplist_node *newnext) noexcept
    {
        expected = marked_ptr(expected);
        return next_[level].compare_exchange_strong(expected, newnext);
    }

    skiplist_node *next_valid(int level) const noexcept
    {
        auto n = next(level);
        while (n.first && (!n.second || n.first->is_marked()))
            n = n.first->next(level);

        return n.first;
    }

    bool mark_next_deleted(int level, skiplist_node *if_matches) noexcept
    {
        return next_[level].compare_exchange_strong(if_matches, marked_ptr(if_matches));
    }

};

}

template<typename T, int TSize, typename TLess = std::less<T>>
class skiplist_reservoir
{
public:
    static constexpr int width = internal::skiplist_node<T, TSize>::width;

private:
    using node = internal::skiplist_node<T, TSize>;
    using node_ptr = typename internal::skiplist_node<T, TSize>::ptr;
    TLess cmp_;
    node_ptr head_;
    node_ptr freelist_head_;

    static std::default_random_engine random_;

    std::pair<node *, node *> find_location(node *before, int level, const T &value) const noexcept;
    node * find_location(int level, const T &value) const noexcept;
    std::pair<node *, node *> find_location(node *before, int level, const T &value) noexcept;
    void find_location(int level, const T &value, std::array<std::pair<node *, node *>, width> &into) noexcept;
    void takeover_head(node *newhead, node *oldhead) noexcept;
    void finish_insert(int level, node *insertnode, std::array<std::pair<node *, node *>, width> &locations) noexcept;
    void remove_node_from_level(int level, node *prev_hint, node *remnode) noexcept;
    node *make_node(const T &value, int level);
    void deallocate(node *nd);

    friend class internal::skiplist_node<T, TSize>;
public:

    class iterator : public std::iterator<std::input_iterator_tag, T>
    {
        friend class skiplist_reservoir;
        internal::skiplist_node<T, TSize> *node_;
        skiplist_reservoir *parent_;

        explicit iterator(skiplist_reservoir *parent, internal::skiplist_node<T, TSize> *node) noexcept;
    public:
        iterator() noexcept;
        iterator(const iterator &iterator) noexcept;
        ~iterator() noexcept;

        iterator &operator++() noexcept;
        bool operator==(const iterator &other) const noexcept;
        bool operator!=(const iterator &other) const noexcept;
        const T &operator*() const noexcept;
        const T *operator->() const noexcept;

        iterator &operator=(const iterator &other) noexcept;
    };

    skiplist_reservoir() noexcept;

    bool insert(const T &value) noexcept;
    iterator begin() noexcept;
    iterator end() const noexcept;
    iterator find(const T &value) noexcept;
};

// used as a macro to retry in atomic loops
#define yield_and_continue() std::this_thread::yield(); continue

template<typename T, int TSize, typename TLess>
skiplist_reservoir<T, TSize, TLess>::iterator::iterator(skiplist_reservoir *parent, internal::skiplist_node<T, TSize> *node) noexcept :
        parent_(parent)
{
    while (node)
    {
        if (node->reference())
            break;

        node = node->next_valid(0);
    }

    node_ = node;
}

template<typename T, int TSize, typename TLess>
skiplist_reservoir<T, TSize, TLess>::iterator::iterator() noexcept :
        node_(nullptr)
{ }

template<typename T, int TSize, typename TLess>
skiplist_reservoir<T, TSize, TLess>::iterator::iterator(const iterator &other) noexcept :
        node_(other.node_),
        parent_(other.parent_)
{
    if (node_)
        node_->reference();
}

template<typename T, int TSize, typename TLess>
skiplist_reservoir<T, TSize, TLess>::iterator::~iterator() noexcept
{
    if (node_)
        node_->dereference(*parent_);
}

template<typename T, int TSize, typename TLess>
typename skiplist_reservoir<T, TSize, TLess>::iterator &skiplist_reservoir<T, TSize, TLess>::iterator::operator++() noexcept
{
    auto node = node_;
    auto pnode = node;

    while (true)
    {
        node = node ? node->next_valid(0) : nullptr;
        if (!node || node->reference())
        {
            if (pnode)
                pnode->dereference(*parent_);
            node_ = node;
            return *this;
        }
    }

    return *this;
}

template<typename T, int TSize, typename TLess>
bool skiplist_reservoir<T, TSize, TLess>::iterator::operator==(const iterator &other) const noexcept
{
    return node_ == other.node_;
};

template<typename T, int TSize, typename TLess>
bool skiplist_reservoir<T, TSize, TLess>::iterator::operator!=(const iterator &other) const noexcept
{
    return node_ != other.node_;
}

template<typename T, int TSize, typename TLess>
const T *skiplist_reservoir<T, TSize, TLess>::iterator::operator->() const noexcept
{
    if (!node_)
        return nullptr;
    return &node_->value();
}

template<typename T, int TSize, typename TLess>
const T &skiplist_reservoir<T, TSize, TLess>::iterator::operator*() const noexcept
{
    return node_->value();
}

template<typename T, int TSize, typename TLess>
typename skiplist_reservoir<T, TSize, TLess>::iterator &skiplist_reservoir<T, TSize, TLess>::iterator::operator=(const iterator &other) noexcept
{
    auto pnode = node_;
    node_ = other.node_;
    if (pnode == node_)
        return *this;

    if (node_)
        node_->reference();
    if (pnode)
        pnode->dereference(parent_);

    return *this;
}

template<typename T, int TSize, typename TLess>
std::default_random_engine skiplist_reservoir<T, TSize, TLess>::random_;

template<typename T, int TSize, typename TLess>
skiplist_reservoir<T, TSize, TLess>::skiplist_reservoir() noexcept :
        head_(nullptr),
        freelist_head_(nullptr)
{
}

template<typename T, int TSize, typename TLess>
bool skiplist_reservoir<T, TSize, TLess>::insert(const T &value) noexcept
{
    std::uniform_int_distribution<int> generator(0, width-1);
    int level = generator(random_);
    std::array<std::pair<node *, node *>, width> locations;

    // 1. root level insert logic
    //    here, the node either becomes the new head or gets
    //    inserted into the appropriate location in all of its
    //    levels
    // find the insert locations
    auto insert_node = make_node(value, level);

    auto head = head_.load();
    while (true)
    {
        if (!head)
        {
            // special case - the list is empty
            if (head_.compare_exchange_strong(head, insert_node))
            {
                // 1a. we just set the new head.
                //      There is no additional housekeeping necessary
                return true;
            }

            yield_and_continue();
        }

        // let the removal of head complete
        if (head->is_marked())
        {
            std::this_thread::yield();
            head = head_.load();
            continue;
        }

        find_location(0, value, locations);

        // 2. We may be inserting a value that is already in the set. If so
        //    we'll just return a proper false value
        //    we establish that by seeing if the value is not less than the "after"
        //    which we already established as not being less than the value
        if (locations[0].second && !cmp_(value, locations[0].second->value()) && !locations[0].second->is_marked())
        {
            std::cout << "inserting " << insert_node << " but already found the value at " << locations[0].second << std::endl;
            return false;
        }

        // 3. There is in fact, already a head. But the value we're inserting
        //    belongs in front of it. So it needs to become the new head
        if (!locations[0].first)
        {
            // for now, we will set our next to be the head on every level
            // that will ensure that if any nodes get inserted after the head
            // while we're operating, we'll catch them. But this means
            // we'll need to go back and clean those up later
            int i = 0;
            for (; i < width; i++)
            {
                if (!insert_node->set_next(i, head) || head->is_marked())
                    break;
            }

            if (!head->is_marked())
            {
                if (head_.compare_exchange_strong(head, insert_node))
                {
                    // we have to finish the takeover as the head
                    // this will keep the state of the list valid
                    // in that there are no lost nodes. But will leave
                    // extra hops on levels beyond the old head's levels
                    takeover_head(insert_node, head);
                    return true;
                }
            }
            else // the head got marked - let's grab the latest head and try again
                head = head_.load();

            yield_and_continue();
        }

        // 4. This is a standard insert. We'll stick the node
        //    where it belongs in level 0. Once we do that. We're good to
        //    set it's other levels and return true
        if (locations[0].first->insert_next(0, locations[0].second, insert_node))
        {
            // success!
            for (int i = 1; i <= level; i++)
                finish_insert(i, insert_node, locations);
            return true;
        }

        head = head_.load();
        yield_and_continue();
    }

    return false;
}

template<typename T, int TSize, typename TLess>
typename skiplist_reservoir<T, TSize, TLess>::iterator skiplist_reservoir<T, TSize, TLess>::begin() noexcept
{
    return iterator(this, head_.load());
}

template<typename T, int TSize, typename TLess>
typename skiplist_reservoir<T, TSize, TLess>::iterator skiplist_reservoir<T, TSize, TLess>::end() const noexcept
{
    return iterator();
}

template<typename T, int TSize, typename TLess>
typename skiplist_reservoir<T, TSize, TLess>::iterator skiplist_reservoir<T, TSize, TLess>::find(const T &value) noexcept
{
    return iterator(this, find_location(0, value));
}

template<typename T, int TSize, typename TLess>
std::pair<typename skiplist_reservoir<T, TSize, TLess>::node *, typename skiplist_reservoir<T, TSize, TLess>::node *>
skiplist_reservoir<T, TSize, TLess>::find_location(node *before, int level, const T &value) noexcept
{
    auto head_pair = [this]() {
        auto head = head_.load();
        return std::make_pair(head, !head->is_marked());
    };
    auto after = before ? before->next(level) : head_pair();
    while (after.first)
    {
        // if our next node is marked for deletion
        // or if it doesn't belong on this level (probably because
        // it used to be head and got moved here)
        if (before && after.first->level() < level)
            after.second = after.second && !before->mark_next_deleted(level, after.first);

        if (!after.second)
        {
            remove_node_from_level(level, before, after.first);
            after = before->next(level);
            continue;
        }

        if (!cmp_(after.first->value(), value))
            break;

        before = after.first;
        after = before->next(level);
    }

    return std::make_pair(before, after.first);
}

template<typename T, int TSize, typename TLess>
std::pair<typename skiplist_reservoir<T, TSize, TLess>::node *, typename skiplist_reservoir<T, TSize, TLess>::node *>
skiplist_reservoir<T, TSize, TLess>::find_location(node *before, int level, const T &value) const noexcept
{
    auto head_pair = [level, this]() {
        auto head = head_.load();
        while (head && head->is_marked())
            head = head->next_valid(level);

        return head;
    };

    auto after = before ? before->next_valid(level) : head_pair();
    while (after)
    {
        if (!cmp_(after->value(), value))
            break;

        before = after;
        after = before->next_valid(level);
    }

    return std::make_pair(before, after);
}


template<typename T, int TSize, typename TLess>
internal::skiplist_node<T, TSize> *skiplist_reservoir<T, TSize, TLess>::find_location(int level, const T &value) const noexcept
{
    node *cbefore = nullptr;
    for (int i = width - 1; i >= level; i--)
    {
        auto fnd = find_location(cbefore, i, value);

        if (fnd.second && !cmp_(fnd.second->value(), value))
            return fnd.second;

        if (!fnd.first)
            return nullptr;

        cbefore = fnd.first;
    }
}

template<typename T, int TSize, typename TLess>
void skiplist_reservoir<T, TSize, TLess>::find_location(
        int level,
        const T &value,
        std::array<std::pair<node *, node *>, width> &into) noexcept
{
    node *cbefore = nullptr;
    for (int i = width - 1; i >= level; i--)
    {
        into[i] = find_location(cbefore, i, value);

        if (!into[i].first)
        {
            auto answer = into[i];
            for (--i; i >= level; i--)
                into[i] = answer;
            break;
        }

        cbefore = into[i].first;
    }
}

template<typename T, int TSize, typename TLess>
internal::skiplist_node<T, TSize> *skiplist_reservoir<T, TSize, TLess>::make_node(const T &value, int level)
{
    // try to get a node out of our free list
    auto flhead = freelist_head_.load();
    while (true)
    {
        if (flhead == nullptr) // nope, nothing in the freelist
            break;

        auto next = flhead->next(0).first;
        if (freelist_head_.compare_exchange_strong(flhead, next))
        {
            flhead->init(value, level);
            return flhead;
        }
    }

    // We had no free nodes
    auto result = new internal::skiplist_node<T, TSize>();
    result->init(value, level);

    return result;
}

template<typename T, int TSize, typename TLess>
void skiplist_reservoir<T, TSize, TLess>::takeover_head(node *newhead, node *oldhead) noexcept
{
    // In this phase, oldhead was the head and newhead is now the head
    // however, all of newhead's next's are pointing to oldhead.
    // this leaves us in a valid state. But it's sub-optimal.
    // So this will make a best effort to not do that.
    for (int i = width - 1; i > oldhead->width; i++)
    {
        // for each level, we are ok for head to be next, but not for anyone's next
        // to be in a transitory invalid state. So we'll just keep trying to remove
        // head from the levels beyond where head should be
        remove_node_from_level(i, newhead, oldhead);
    }
}

template<typename T, int TSize, typename TLess>
void skiplist_reservoir<T, TSize, TLess>::remove_node_from_level(int level, node *prev_hint, node *remnode) noexcept
{
    while (true)
    {
        auto pnext = prev_hint->next(level);
        if (pnext.first != remnode)
        {
            // our prev_hint is no longer pointing to remnode
            // but prev_hint *must* be non-null and must be
            // logically in front of remnode
            while (pnext.first)
            {
                prev_hint = pnext.first;
                pnext = pnext.first->next(level);
                if (pnext.first == remnode)
                    break;

                if (!pnext.first || cmp_(pnext.first->value(), remnode->value()))
                {
                    // if our removing node is less than our current previous node
                    // then it's already gone
                    return;
                }
            }
        }

        // So we'll find our next node that we'll take over.
        // We're going to leave it in remnode but mark the
        // next ptr as being deleted so it won't get replaced
        // the attempt to remove it will be fine. Removing
        // a node won't change the removing node's pointers
        // TODO verify that
        auto new_next = remnode->next(level);
        while (new_next.first && (!new_next.second || new_next.first->is_marked()))
        {
            remove_node_from_level(level, remnode, new_next.first);
            new_next = remnode->next(level);
        }

        remnode->mark_next_deleted(level, new_next.first);

        auto cmpnode = remnode;
        // first make sure we cmpxchg that node
        if (!prev_hint->remove_next(level, remnode, new_next.first))
        {
            yield_and_continue();
        }

        return;
    }
}

template<typename T, int TSize, typename TLess>
void skiplist_reservoir<T, TSize, TLess>::finish_insert(
        int level,
        node *insertnode,
        std::array<std::pair<node *, node *>, width> &locations) noexcept
{
    // in this function, insertnode has already
    // been inserted at level 0 - but it needs to be inserted
    // at all of the the subsequent levels
    // which we'll do as long as the node isn't marked
    while (!insertnode->is_marked())
    {
        if (locations[level].first->insert_next(level, locations[level].second, insertnode))
            return;

        // that failed. We need to rescan the locations for the level
        find_location(level, insertnode->value(), locations);
    }
}

template<typename T, int TSize, typename TLess>
void skiplist_reservoir<T, TSize, TLess>::deallocate(node *nd)
{
    // we can drop this into the freelist
    auto flhead = freelist_head_.load();
    while (true)
    {
        nd->set_next(0, flhead);

        if (freelist_head_.compare_exchange_strong(flhead, nd))
            break;
    }
}
#undef yield_and_continue
}

#endif //CXXMETRICS_PQSKIPLIST_HPP
