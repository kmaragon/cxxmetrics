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
    constexpr static skiplist_node *marked_ptr(skiplist_node *ptr) noexcept
    {
        return reinterpret_cast<skiplist_node *>(reinterpret_cast<unsigned long>(ptr) | DELETE_MARKER);
    }

    constexpr static skiplist_node *unmarked_ptr(skiplist_node *ptr) noexcept
    {
        return reinterpret_cast<skiplist_node *>(reinterpret_cast<unsigned long>(ptr) & ~DELETE_MARKER);
    }

    constexpr static bool ptr_is_marked(skiplist_node *ptr) noexcept
    {
        return (reinterpret_cast<unsigned long>(ptr) & DELETE_MARKER) != 0;
    }

#ifndef NDEBUG
    constexpr auto refcnt() const noexcept
    {
        return refs_.load();
    }
#endif

    int level() const noexcept
    {
        return level_.load() & ~LEVEL_DELETE_MARKER;
    }

    static inline void print_backtrace(std::ostream &to)
    {
        void *bt[10];
        char *demangle_buf = (char *)malloc(512);

        auto bsz = backtrace(bt, 10);
        auto bs = backtrace_symbols(bt, bsz);
        for (int i = 0; i < bsz; i++)
        {
            size_t len = 512;
            int status;

            auto sname = bs[i];
            if (strstr(sname, "print_backtrace"))
                continue;

            while (*sname)
            {
                if (*(sname++) == '(')
                    break;
            }

            char atchr = 0;
            char *atloc = nullptr;
            if (*sname)
            {
                for (char *at = sname; *at; ++at)
                {
                    if (*at == ')' || *at == '+')
                    {
                        atchr = *at;
                        *at = 0;
                        atloc = at + 1;
                        break;
                    }
                }

                abi::__cxa_demangle(sname, demangle_buf, &len, &status);
                if (!status)
                {
                    demangle_buf[len] = 0;
                    to << demangle_buf;
                    if (atloc)
                        to << " at (" << atloc;
                    to << std::endl;
                    continue;
                }
            }

            if (atloc)
                *(atloc-1) = atchr;

            to << bs[i] << std::endl;
        }

        free(demangle_buf);
        free(bs);
    }

    bool reference() noexcept
    {
        auto refs = refs_.load();
        while (true)
        {
            if (refs == 0)
                return false;

            if (refs_.compare_exchange_strong(refs, refs + 1))
            {
                /* backtrace
                std::stringstream str;
                str << "Thread " << std::this_thread::get_id() << std::endl;
                str << "reference to " << (refs + 1) << " value " << value() << " level " << level() << " at:" << std::endl;
                print_backtrace(str);
                std::cout << str.str() << std::endl << std::endl;
                /* /backtrace */

                return true;
            }
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

                /* backtrace
                std::stringstream str;
                str << "Thread " << std::this_thread::get_id() << std::endl;
                str << "dereference to " << (refs - 1) << " value " << value() << (refs == 1 ? " (deleted)" : " ") << " level " << level() << " at:" << std::endl;
                print_backtrace(str);
                std::cout << str.str() << std::endl << std::endl;
                /* /backtrace */
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
        }
        return next_[level].compare_exchange_strong(next, node);
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
        while (n.first && n.first->is_marked())
            n = n.first->next(level);

        return n.first;
    }

    bool mark_next_deleted(int level, skiplist_node *&if_matches) noexcept
    {
        if (ptr_is_marked(if_matches))
        {
            if_matches = unmarked_ptr(next_[level].load());
            return false;
        }

        bool result = next_[level].compare_exchange_strong(if_matches, marked_ptr(if_matches));
        if_matches = unmarked_ptr(if_matches);
        return result;
    }

};

template<typename T, int TSize, typename TAlloc>
class skiplist_node_pin;

template<typename T, int TSize, typename TAlloc>
skiplist_node_pin<T, TSize, TAlloc> pin_node(skiplist_node<T, TSize> *node, TAlloc &allocator) noexcept;

template<typename T, int TSize, typename TAlloc>
class skiplist_node_pin
{
    skiplist_node<T, TSize> *ptr_;
    TAlloc *alloc_;

    skiplist_node_pin(skiplist_node<T, TSize> *node, TAlloc &allocator) noexcept;

    friend skiplist_node_pin<T, TSize, TAlloc> pin_node<T, TSize, TAlloc>(skiplist_node<T, TSize> *node, TAlloc &allocator) noexcept;
public:
    skiplist_node_pin() noexcept;
    skiplist_node_pin(const skiplist_node_pin &) noexcept;
    skiplist_node_pin(skiplist_node_pin &&mv) noexcept;
    ~skiplist_node_pin() noexcept;

    skiplist_node_pin &operator=(const skiplist_node_pin &cp) noexcept;
    skiplist_node_pin &operator=(skiplist_node_pin &&mv) noexcept;
    skiplist_node<T, TSize> *operator->() const noexcept;
    operator bool() const noexcept;
    bool operator==(const skiplist_node_pin &other) const noexcept;
    bool operator!=(const skiplist_node_pin &other) const noexcept;

    TAlloc &allocator() noexcept
    {
        return *alloc_;
    }

    skiplist_node<T, TSize> *get() const noexcept
    {
        return ptr_;
    };
};

template<typename T, int TSize, typename TAlloc>
skiplist_node_pin<T, TSize, TAlloc> pin_node(skiplist_node<T, TSize> *node, TAlloc &allocator) noexcept
{
    if (node && !node->reference())
        return skiplist_node_pin<T, TSize, TAlloc>();

    return std::move(skiplist_node_pin<T, TSize, TAlloc>(node, allocator));
}

template<typename T, int TSize, typename TAlloc>
skiplist_node_pin<T, TSize, TAlloc>::skiplist_node_pin(skiplist_node<T, TSize> *node, TAlloc &allocator) noexcept :
        ptr_(node),
        alloc_(&allocator)
{ }

template<typename T, int TSize, typename TAlloc>
skiplist_node_pin<T, TSize, TAlloc>::skiplist_node_pin() noexcept :
        ptr_(nullptr),
        alloc_(nullptr)
{ }

template<typename T, int TSize, typename TAlloc>
skiplist_node_pin<T, TSize, TAlloc>::skiplist_node_pin(const skiplist_node_pin &other) noexcept
{
    ptr_ = other.ptr_;
    if (ptr_ && !ptr_->reference())
        ptr_ = nullptr;

    alloc_ = other.alloc_;
}

template<typename T, int TSize, typename TAlloc>
skiplist_node_pin<T, TSize, TAlloc>::skiplist_node_pin(skiplist_node_pin &&mv) noexcept :
        ptr_(mv.ptr_),
        alloc_(mv.alloc_)
{
    mv.ptr_ = nullptr;
    mv.alloc_ = nullptr;
}

template<typename T, int TSize, typename TAlloc>
skiplist_node_pin<T, TSize, TAlloc>::~skiplist_node_pin() noexcept
{
    if (ptr_ && alloc_)
        ptr_->dereference(*alloc_);
}


template<typename T, int TSize, typename TAlloc>
skiplist_node_pin<T, TSize, TAlloc> &skiplist_node_pin<T, TSize, TAlloc>::operator=(const skiplist_node_pin &cp) noexcept
{
    auto cpptr = cp.ptr_;
    if (ptr_ == cpptr)
        return *this;

    if (cpptr)
        cpptr->reference();

    if (ptr_ && alloc_)
        ptr_->dereference(*alloc_);

    ptr_ = cpptr;
    alloc_ = cp.alloc_;

    return *this;
}

template<typename T, int TSize, typename TAlloc>
skiplist_node_pin<T, TSize, TAlloc> &skiplist_node_pin<T, TSize, TAlloc>::operator=(skiplist_node_pin &&mv) noexcept
{
    auto mvptr = mv.ptr_;
    if (ptr_ == mvptr)
        return *this;

    mv.ptr_ = nullptr;
    if (ptr_ && alloc_)
        ptr_->dereference(*alloc_);

    ptr_ = mvptr;
    alloc_ = mv.alloc_;

    mv.alloc_ = nullptr;

    return *this;
}

template<typename T, int TSize, typename TAlloc>
skiplist_node<T, TSize> *skiplist_node_pin<T, TSize, TAlloc>::operator->() const noexcept
{
    return ptr_;
}

template<typename T, int TSize, typename TAlloc>
skiplist_node_pin<T, TSize, TAlloc>::operator bool() const noexcept
{
    return (ptr_ != nullptr);
}

template<typename T, int TSize, typename TAlloc>
bool skiplist_node_pin<T, TSize, TAlloc>::operator==(const skiplist_node_pin &other) const noexcept
{
    return (ptr_ == other.ptr_);
}

template<typename T, int TSize, typename TAlloc>
bool skiplist_node_pin<T, TSize, TAlloc>::operator!=(const skiplist_node_pin &other) const noexcept
{
    return (ptr_ != other.ptr_);
}

}

template<typename T, int TSize, typename TLess = std::less<T>>
class skiplist
{
public:
    static constexpr int width = internal::skiplist_node<T, TSize>::width;

private:
    using pin = internal::skiplist_node_pin<T, TSize, skiplist>;
    using node = internal::skiplist_node<T, TSize>;
    using node_ptr = typename internal::skiplist_node<T, TSize>::ptr;
    TLess cmp_;
    node_ptr head_;
    node_ptr freelist_head_;

    static std::default_random_engine random_;

    std::pair<node *, node *> find_location(node *before, int level, const T &value) const noexcept;
    node * find_location(int level, const T &value) const noexcept;
    std::pair<pin, pin> find_location(pin &before, int level, const T &value) noexcept;
    void find_location(int level, const T &value, std::array<std::pair<pin, pin>, width> &into) noexcept;
    void takeover_head(pin &newhead, pin &oldhead) noexcept;
    void finish_insert(int level, pin &insertnode, std::array<std::pair<pin, pin>, width> &locations) noexcept;
    void remove_node_from_level(int level, const pin &prev_hint, const pin &remnode) noexcept;
    node *make_node(const T &value, int level);
    void deallocate(node *nd);

    auto pin_next(int level, const pin &node)
    {
        while (true)
        {
            if (!node)
                return std::move(std::make_pair(pin(), false));

            auto nxt = node->next(level);
            if (!nxt.first)
                return std::move(std::make_pair(pin(), nxt.second));

            auto pinned = std::move(internal::pin_node(nxt.first, *this));
            if (pinned)
                return std::move(std::make_pair(std::move(pinned), nxt.second));

            if (node->is_marked())
                return std::move(std::make_pair(pin(), nxt.second));
        }
    }

    friend class internal::skiplist_node<T, TSize>;
public:

    class iterator : public std::iterator<std::input_iterator_tag, T>
    {
        friend class skiplist;
        internal::skiplist_node_pin<T, TSize, skiplist> node_;

        explicit iterator(internal::skiplist_node_pin<T, TSize, skiplist> &&node) noexcept;
    public:
        iterator() noexcept;
        iterator(const iterator &other) noexcept;
        ~iterator() noexcept;

        iterator &operator++() noexcept;
        bool operator==(const iterator &other) const noexcept;
        bool operator!=(const iterator &other) const noexcept;
        const T &operator*() const noexcept;
        const T *operator->() const noexcept;

        iterator &operator=(const iterator &other) noexcept;
    };

    skiplist() noexcept;
    ~skiplist();

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
skiplist<T, TSize, TLess>::iterator::iterator(internal::skiplist_node_pin<T, TSize, skiplist> &&node) noexcept :
        node_(std::move(node))
{ }

template<typename T, int TSize, typename TLess>
skiplist<T, TSize, TLess>::iterator::iterator() noexcept
{ }

template<typename T, int TSize, typename TLess>
skiplist<T, TSize, TLess>::iterator::iterator(const iterator &other) noexcept :
    node_(other.node_)
{
};

template<typename T, int TSize, typename TLess>
skiplist<T, TSize, TLess>::iterator::~iterator() noexcept
{
}

template<typename T, int TSize, typename TLess>
typename skiplist<T, TSize, TLess>::iterator &skiplist<T, TSize, TLess>::iterator::operator++() noexcept
{
    while (true)
    {
        if (!node_)
            return *this;

        auto nextptr = node_->next_valid(0);
        if (!nextptr)
        {
            node_ = std::move(pin());
            return *this;
        }

        auto nn = internal::pin_node(nextptr, node_.allocator());
        if (nn)
        {
            node_ = std::move(nn);
            return *this;
        }

        if (node_->is_marked())
        {
            node_ = std::move(pin());
            return *this;
        }
    }
}

template<typename T, int TSize, typename TLess>
bool skiplist<T, TSize, TLess>::iterator::operator==(const iterator &other) const noexcept
{
    return node_ == other.node_;
};

template<typename T, int TSize, typename TLess>
bool skiplist<T, TSize, TLess>::iterator::operator!=(const iterator &other) const noexcept
{
    return node_ != other.node_;
}

template<typename T, int TSize, typename TLess>
const T *skiplist<T, TSize, TLess>::iterator::operator->() const noexcept
{
    if (!node_)
        return nullptr;
    return &node_->value();
}

template<typename T, int TSize, typename TLess>
const T &skiplist<T, TSize, TLess>::iterator::operator*() const noexcept
{
    return node_->value();
}

template<typename T, int TSize, typename TLess>
typename skiplist<T, TSize, TLess>::iterator &skiplist<T, TSize, TLess>::iterator::operator=(const iterator &other) noexcept
{
    node_ = other.node_;
    return *this;
}

template<typename T, int TSize, typename TLess>
skiplist<T, TSize, TLess>::~skiplist()
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
std::default_random_engine skiplist<T, TSize, TLess>::random_;

template<typename T, int TSize, typename TLess>
skiplist<T, TSize, TLess>::skiplist() noexcept :
        head_(nullptr),
        freelist_head_(nullptr)
{
}

template<typename T, int TSize, typename TLess>
bool skiplist<T, TSize, TLess>::erase(const iterator &value) noexcept
{
    if (value == end())
        return false;

    auto &rmnode = value.node_;

    // first mark the node as being deleted
    if (!rmnode->mark_for_deletion())
        return false;

    // now go through and remove it from the top level down to the bottom
    pin cbefore;
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

    return true;
}

template<typename T, int TSize, typename TLess>
bool skiplist<T, TSize, TLess>::insert(const T &value) noexcept
{
    std::uniform_int_distribution<int> generator(0, width-1);
    int level = generator(random_);
    std::array<std::pair<pin, pin>, width> locations;

    // 1. root level insert logic
    //    here, the node either becomes the new head or gets
    //    inserted into the appropriate location in all of its
    //    levels
    // find the insert locations
    auto insnode = make_node(value, level);
    auto insert_node = std::move(internal::pin_node(insnode, *this));

    auto get_head = [this]() {
        while (true)
        {
            auto headptr = head_.load();
            if (!headptr)
                return std::move(pin());

            auto hptr = std::move(internal::pin_node(headptr, *this));
            if (hptr)
                return std::move(hptr);
        }
    };
    auto head = std::move(get_head());

    while (true)
    {
        if (!head)
        {
            // special case - the list is empty
            node *current_head = nullptr;
            for (int i = 1; i < width; i++)
                insert_node->reference();

            if (head_.compare_exchange_strong(current_head, insert_node.get()))
            {
                // 1a. we just set the new head.
                //      There is no additional housekeeping necessary
                return true;
            }

            // doh! that didn't work
            for (int i = 1; i < width; i++)
                insert_node->dereference(*this);

            assert(insert_node->refcnt() == 2);
            head = std::move(internal::pin_node(current_head, *this));
            if (!head)
                head = std::move(get_head());

            yield_and_continue();
        }

        find_location(0, value, locations);

        // 2. We may be inserting a value that is already in the set. If so
        //    we'll just return a proper false value
        //    we establish that by seeing if the value is not less than the "after"
        //    which we already established as not being less than the value
        if (locations[0].second && !cmp_(value, locations[0].second->value()) && !locations[0].second->is_marked())
        {
            // free the node - we dereference it because it's never added to the list
            insert_node->dereference(*this);
            return false;
        }

        // 3. There is in fact, already a head. But the value we're inserting
        //    belongs in front of it. So it needs to become the new head
        if (!locations[0].first || (locations[0].first == head && head->is_marked()))
        {
            // for now, we will set our next to be the head on every level
            // that will ensure that if any nodes get inserted after the head
            // while we're operating, we'll catch them. But this means
            // we'll need to go back and clean those up later
            // point the nexts to the current head and add the references

            // for the node to be head
            insert_node->set_next(0, head.get());
            for (int i = 1; i < width; i++)
            {
                insert_node->set_next(i, head.get());
                insert_node->reference();
            }

            node *current_head = head.get();
            assert(insert_node->refcnt() >= width);
            if (head_.compare_exchange_strong(current_head, insert_node.get()))
            {
                // we have to finish the takeover as the head
                // this will keep the state of the list valid
                // in that there are no lost nodes. But will leave
                // extra hops on levels beyond the old head's levels
                takeover_head(insert_node, head);

                // let the insert node get deleted
                return true;
            }

            // we failed to make the node head, reset it's refcount to 1
            for (int i = 1; i < width; i++)
                insert_node->dereference(*this);

            assert(insert_node->refcnt() == 2);
            head = std::move(internal::pin_node(current_head, *this));
            if (!head)
                head = std::move(get_head());

            yield_and_continue();
        }
        if (locations[0].first->is_marked())
        {
            // ok - well the node got marked - let's try again
            head = std::move(get_head());
            yield_and_continue();
        }

        // 4. This is a standard insert. We'll stick the node
        //    where it belongs in level 0. Once we do that. We're good to
        //    set it's other levels and return true
        if (locations[0].first->insert_next(0, locations[0].second.get(), insert_node.get()))
        {
            // success!
            for (int i = 1; i <= level; i++)
                finish_insert(i, insert_node, locations);

            // let the node get deleted when it's time
            // the reference count is already one from making the node.
            // And it'll bump up with the finish_insert calls
            return true;
        }

        // now first is safe for removal until we pick it up again
        head = std::move(get_head());
        yield_and_continue();
    }
}

template<typename T, int TSize, typename TLess>
typename skiplist<T, TSize, TLess>::iterator skiplist<T, TSize, TLess>::begin() noexcept
{
    while (true)
    {
        auto head = head_.load();
        if (head->is_marked())
            head = head->next_valid(0);

        if (!head)
            return iterator();

        auto pp = internal::pin_node<T, TSize, skiplist>(head, *this);
        if (pp)
            return iterator(std::move(pp));
    }
}

template<typename T, int TSize, typename TLess>
typename skiplist<T, TSize, TLess>::iterator skiplist<T, TSize, TLess>::end() const noexcept
{
    return iterator();
}

template<typename T, int TSize, typename TLess>
typename skiplist<T, TSize, TLess>::iterator skiplist<T, TSize, TLess>::find(const T &value) noexcept
{
    auto fndNode = find_location(0, value);
    if (!fndNode)
        return iterator();

    return iterator(std::move(internal::pin_node(fndNode, *this)));
}

template<typename T, int TSize, typename TLess>
std::pair<internal::skiplist_node_pin<T, TSize, skiplist<T, TSize, TLess>>, internal::skiplist_node_pin<T, TSize, skiplist<T, TSize, TLess>>>
skiplist<T, TSize, TLess>::find_location(pin &before, int level, const T &value) noexcept
{
    auto head_pair = [this]() {
        auto head = head_.load();
        while (head)
        {
            auto hd = std::move(internal::pin_node(std::move(head), *this));
            if (hd)
                return std::move(std::make_pair(std::move(hd), !head->is_marked()));
        }

        return std::move(std::make_pair(pin(), false));
    };

    auto after = before ? pin_next(level, before) : head_pair();
    while (after.first)
    {
        // if our next node is marked for deletion
        // or if it doesn't belong on this level (probably because
        // it used to be head and got moved here)
        if (before && (after.first->is_marked() || after.first->level() < level))
        {
            remove_node_from_level(level, before, after.first);
            after = std::move(pin_next(level, before));
            continue;
        }

        if (!cmp_(after.first->value(), value))
            break;

        before = std::move(after.first);
        after = std::move(pin_next(level, before));
    }

    return std::move(std::make_pair(std::move(before), std::move(after.first)));
}

template<typename T, int TSize, typename TLess>
std::pair<typename skiplist<T, TSize, TLess>::node *, typename skiplist<T, TSize, TLess>::node *>
skiplist<T, TSize, TLess>::find_location(node *before, int level, const T &value) const noexcept
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
internal::skiplist_node<T, TSize> *skiplist<T, TSize, TLess>::find_location(int level, const T &value) const noexcept
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
void skiplist<T, TSize, TLess>::find_location(
        int level,
        const T &value,
        std::array<std::pair<pin, pin>, width> &into) noexcept
{
    pin cbefore;
    for (int i = width - 1; i >= level; i--)
    {
        into[i] = std::move(find_location(cbefore, i, value));
        if (!into[i].first)
        {
            auto &answer = into[i];
            for (--i; i >= level; i--)
                into[i] = answer;
            break;
        }

        cbefore = into[i].first;
    }
}

template<typename T, int TSize, typename TLess>
internal::skiplist_node<T, TSize> *skiplist<T, TSize, TLess>::make_node(const T &value, int level)
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
void skiplist<T, TSize, TLess>::takeover_head(pin &newhead, pin &oldhead) noexcept
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
        assert(oldhead->refcnt() > oldhead->level());
    }
}

template<typename T, int TSize, typename TLess>
void skiplist<T, TSize, TLess>::remove_node_from_level(int level, const pin &prev_hint, const pin &remnode) noexcept
{
    pin prev = prev_hint;
    while (true)
    {
        auto rmnode_nmatch = remnode;
        node *rmnode_node = rmnode_nmatch.get();

        // step 1: resolve prev
        while (!prev->mark_next_deleted(level, rmnode_node))
        {
            // either prev_hint isn't pointing to rmnode_node, or
            // someone else marked the node deleted
            rmnode_nmatch = std::move(internal::pin_node(rmnode_node, *this));
            if (rmnode_nmatch == remnode)
            {
                // the node has already been marked
                // we'll go in for the assist and delete it
                // but only one should remove the reference
                // someone else deleted the node before us
                break;
            }
            else if (!rmnode_node || (rmnode_nmatch && !cmp_(rmnode_nmatch->value(), remnode->value())))
            {
                // the node has already been removed
                return;
            }

            if (rmnode_nmatch)
                prev = rmnode_nmatch;

            if (prev->next(level).first && prev->next(level).first->refcnt() == 0)
            {
                std::cout << "Node " << prev->next(level).first << ", value " << prev->next(level).first->value() << " was prematurely freed" << std::endl;
                std::cout << "While getting next on level " << level << " for " << prev->value() << std::endl;
                abort();
            }
        }

        // So we'll find our next node that will take over.
        // We're going to leave it in remnode but mark the
        // next ptr as being deleted so it won't get replaced
        // the attempt to remove it will be fine. Removing
        // a node won't change the removing node's pointers
        auto new_next = std::move(pin_next(level, remnode));
        while (new_next.first && (new_next.first->is_marked() || new_next.first->level() < level))
        {
            remove_node_from_level(level, remnode, new_next.first);
            new_next = std::move(pin_next(level, remnode));
        }

        auto newnext = new_next.first.get();
        if (!remnode->mark_next_deleted(level, newnext))
        {
            // ensure that the next is marked as deleted
            // to in-turn, ensure that no one else swaps another node
            // in here
            if (newnext != new_next.first.get())
            {
                yield_and_continue();
            }
        }

        // first make sure we cmpxchg that node
        assert(remnode != new_next.first);
        if (!prev->remove_next(level, remnode.get(), new_next.first.get()))
        {
            // prev has a different next now. Start over
            yield_and_continue();
        }

        // we were the ones that removed the node from it's prev
        // we should deref it
        /* backtrace
        std::stringstream str;
        str << "Thread " << std::this_thread::get_id() << std::endl;
        str << "dereference node " << remnode.get() << " value " << remnode->value() << " level " << level << " of " << remnode->level() << std::endl;
        str << "Prev = " << prev.get() << " (" << prev->value() << ")" << std::endl;
        str << "Replaced by " << (new_next.first ? new_next.first.get() : nullptr) << " (" << (new_next.first ? new_next.first->value() : -1) << ")" << std::endl;
        node::print_backtrace(str);
        std::cout << str.str() << std::endl << std::endl;
        /* /backtrace */
        // dereference the node for this level if it's truly gone
        remnode->dereference(*this);
        return;
    }
}

template<typename T, int TSize, typename TLess>
void skiplist<T, TSize, TLess>::finish_insert(
        int level,
        pin &insertnode,
        std::array<std::pair<pin, pin>, width> &locations) noexcept
{
    // in this function, insertnode has already
    // been inserted at level 0 - but it needs to be inserted
    // at all of the the subsequent levels
    // which we'll do as long as the node isn't marked
    while (!insertnode->is_marked())
    {
        insertnode->reference();
        if (locations[level].first && locations[level].first->insert_next(level, locations[level].second.get(), insertnode.get()))
        {
            // reference the node for this level
            return;
        }

        insertnode->dereference(*this);

        // that failed. We need to rescan the locations for the level
        find_location(level, insertnode->value(), locations);
        if (!locations[level].first)
        {
            // all the possible predecessors for the node have been removed.
            // that means this one will have had to have been set up
            break;
        }
    }
}

template<typename T, int TSize, typename TLess>
void skiplist<T, TSize, TLess>::deallocate(node *nd)
{
    // TODO - this is called prematurely
    //        all local use of nodes needs to be pinned to prevent this
    //nd->init(0, -1);
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
