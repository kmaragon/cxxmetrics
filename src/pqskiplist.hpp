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
    static constexpr uint16_t LEVEL_DELETE_MARKER = (uint16_t)1 << ((sizeof(uint16_t) * 8) - 1);
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
        auto refs = refs_.load();
        while (true)
        {
            if (refs == 0)
                return;

            if (refs_.compare_exchange_strong(refs, refs - 1))
            {
                // we successfully dropped the references
                if (refs == 1)
                    t.deallocate(this);
                return;
            }
        }
    }

    bool is_marked() const noexcept
    {
        return ((uint16_t)level_.load() & LEVEL_DELETE_MARKER) != 0;
    }

    bool mark_for_deletion() noexcept
    {
        auto nlevel = level_.load();
        while (true)
        {
            if (nlevel & LEVEL_DELETE_MARKER)
                return false;

            if (level_.compare_exchange_strong(nlevel, nlevel | LEVEL_DELETE_MARKER))
                return true;
        }
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
        return (node == nullptr) || !node->is_marked();
    }

    // inserts the given node as the next node after this
    bool insert_next(int level, skiplist_node *next, skiplist_node *node) noexcept
    {
        if (node != nullptr)
        {
            node->next_[level] = next;
            assert(node->value() > value());
        }
        return next_[level].compare_exchange_strong(next, node);
    }

    // replace the next node with newnext if expected is the next at this level
    // requires that the next node has already been marked for deletion
    bool remove_next(int level, skiplist_node *expected, skiplist_node *newnext) noexcept
    {
        expected = marked_ptr(expected);
        assert(!newnext || newnext->value() > value());
        return next_[level].compare_exchange_strong(expected, newnext);
    }

    skiplist_node *next_valid(int level) const noexcept
    {
        auto n = next(level);
        while (n.first && n.first->is_marked())
            n = n.first->next(level);

        return n.first;
    }

    bool mark_next_deleted(int level, skiplist_node *&if_matches) noexcept
    {
        bool result = next_[level].compare_exchange_strong(if_matches, marked_ptr(if_matches));
        if_matches = unmarked_ptr(if_matches);
        return result;
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
    ~skiplist_reservoir();

    bool erase(const T &value) noexcept
    {
        return erase(find(value));
    }

    bool erase(const iterator &value) noexcept;
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
        pnode->dereference(*parent_);

    return *this;
}

template<typename T, int TSize, typename TLess>
skiplist_reservoir<T, TSize, TLess>::~skiplist_reservoir()
{
    auto flhead = freelist_head_.exchange(nullptr);
    auto head = head_.exchange(nullptr);

    while (head)
    {
        auto next = head->next(0);
        delete head;
        head = next.first;
    }

    while (flhead)
    {
        auto next = flhead->next(0);
        delete flhead;
        flhead = next.first;
    }
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
bool skiplist_reservoir<T, TSize, TLess>::erase(const iterator &value) noexcept
{
    if (value == end())
        return false;

    auto rmnode = value.node_;

    // first mark the node as being deleted
    if (!rmnode->mark_for_deletion())
        return false;

    // the iterator should do this - but we take a const iterator reference
    // so let's just be safe
    if (!rmnode->reference())
        return false;

    // now go through and remove it from the top level down to the bottom
    node *cbefore = nullptr;
    for (int i = width - 1; i >= 0; i--)
    {
        while (true)
        {
            auto location = find_location(cbefore, i, rmnode->value());
            cbefore = location.first;

            // if the node isn't on this level, just break
            if (location.second != rmnode || !location.first)
                break;

            // we marked the node as deleted... now let's remove it
            remove_node_from_level(i, location.first, location.second);
            break;
        }
    }

    // it's safe to free now
    rmnode->dereference(*this);
    return true;
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

    // make sure the node isn't deleted before insert finishes
    insert_node->reference();

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
                insert_node->dereference(*this);
                return true;
            }

            yield_and_continue();
        }

        find_location(0, value, locations);

        // 2. We may be inserting a value that is already in the set. If so
        //    we'll just return a proper false value
        //    we establish that by seeing if the value is not less than the "after"
        //    which we already established as not being less than the value
        if (locations[0].second && !cmp_(value, locations[0].second->value()) && !locations[0].second->is_marked())
        {
            // free the node - the basic dereference + the 1 we added to ensure it lives through this function
            insert_node->dereference(*this);
            insert_node->dereference(*this);
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

            if (head_.compare_exchange_strong(head, insert_node))
            {
                // we have to finish the takeover as the head
                // this will keep the state of the list valid
                // in that there are no lost nodes. But will leave
                // extra hops on levels beyond the old head's levels
                takeover_head(insert_node, head);

                // let the insert node get deleted
                insert_node->dereference(*this);
                return true;
            }

            yield_and_continue();
        }

        // we are inserting the node after first - so we'll take a reference to it to make sure
        // it's not deleted while we're using it
        if (!locations[0].first->reference())
        {
            yield_and_continue();
        }

        if (locations[0].first->is_marked())
        {
            // ok - well the node got marked - let's try again
            locations[0].first->dereference(*this);
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

            std::stringstream buf;
            buf << "Inserted " << value << " between " << locations[0].first->value() << " (" << (locations[0].first->is_marked() ? "" : "not ") << "marked) and ";
            if (locations[0].second)
                buf << locations[0].second->value();
            else
                buf << "null";

            // let the node get deleted when it's time
            insert_node->dereference(*this);

            // now first is safe for removal
            locations[0].first->dereference(*this);
            return true;
        }

        // now first is safe for removal until we pick it up again
        locations[0].first->dereference(*this);

        head = head_.load();
        yield_and_continue();
    }
}

template<typename T, int TSize, typename TLess>
typename skiplist_reservoir<T, TSize, TLess>::iterator skiplist_reservoir<T, TSize, TLess>::begin() noexcept
{
    auto head = head_.load();
    if (head->is_marked())
        head = head->next_valid(0);
    return iterator(this, head);
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
    assert(before == nullptr || before->value() < value);

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
        if (before && (after.first->is_marked() || after.first->level() < level))
        {
            remove_node_from_level(level, before, after.first);
            after = before->next(level);
            continue;
        }

        if (!cmp_(after.first->value(), value))
            break;

        before = after.first;
        after = before->next(level);

        assert(before == nullptr || before->value() < value);
    }

    assert(before == nullptr || before->value() < value);
    assert(after.first == nullptr || after.first->value() >= value);
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
        after = after->next_valid(level);
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

        if (fnd.second && !cmp_(value, fnd.second->value()))
            return fnd.second;

        cbefore = fnd.first;
    }

    return nullptr;
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
    for (int i = width - 1; i > oldhead->level(); i--)
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
        auto rmnode_nmatch = remnode;
        while (!prev_hint->mark_next_deleted(level, rmnode_nmatch))
        {
            if (rmnode_nmatch == remnode)
            {
                // the node has already been marked
                // but it still hasn't been deleted.
                // for the assist
                break;
            }

            if (!rmnode_nmatch || !cmp_(rmnode_nmatch->value(), remnode->value()))
            {
                // the node has already been removed
                return;
            }

            prev_hint = rmnode_nmatch;
        }

        // So we'll find our next node that we'll take over.
        // We're going to leave it in remnode but mark the
        // next ptr as being deleted so it won't get replaced
        // the attempt to remove it will be fine. Removing
        // a node won't change the removing node's pointers
        auto new_next = remnode->next(level);
        while (new_next.first && new_next.first->is_marked())
        {
            remove_node_from_level(level, remnode, new_next.first);
            new_next = remnode->next(level);
        }

        auto newnext = new_next.first;
        if (!remnode->mark_next_deleted(level, newnext))
        {
            // ensure that the next is marked as deleted
            // to in-turn, ensure that no one else swaps another node
            // in here
            if (newnext != new_next.first)
            {
                yield_and_continue();
            }
        }

        // first make sure we cmpxchg that node
        if (!prev_hint->remove_next(level, remnode, new_next.first))
        {
            yield_and_continue();
        }

        // we removed the node - let's make sure that we clean up the node
        // if this is the last level it's getting removed from
        if (level == 0)
            remnode->dereference(*this);
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
    nd->init(0, -1);
    return;
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
