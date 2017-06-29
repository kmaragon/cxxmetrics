#ifndef CXXMETRICS_POOL_HPP
#define CXXMETRICS_POOL_HPP

#include <atomic>

namespace cxxmetrics
{

namespace internal
{

template<typename TValue, template<typename ...> class TAlloc = std::allocator>
class pool;

/**
 * \brief A *_ptr style handle that is reference counted and recycled from a pool
 *
 * \tparam TValue The type of value in the ptr
 * \tparam TAlloc The allocator that the pool from whence the ptr derives uses to allocate
 */
template<typename TValue, template<typename ...> class TAlloc = std::allocator>
class pool_ptr
{
    struct pool_data
    {
        typename std::aligned_storage<sizeof(TValue)>::type tv;
        std::atomic<pool_data *> next;
        pool<TValue, TAlloc> *source;

        pool_data(pool<TValue, TAlloc> *src, pool_data *n) :
                source(src),
                next(n)
        { }

        TValue &value()
        {
            return *reinterpret_cast<TValue*>(std::addressof(tv));
        }

        bool is_counted() const
        {
            return next.load() & 1;
        }

        bool add_reference()
        {
            pool_data *ptr = next.load();
            while (true)
            {
                // make sure we are counting and we have a count already
                if ((reinterpret_cast<unsigned long>(ptr) & 1) && (reinterpret_cast<unsigned long>(ptr) & ~((unsigned long)1)))
                {
                    auto *incr = reinterpret_cast<pool_data *>(reinterpret_cast<unsigned long>(ptr) + 0x10);
                    if (next.compare_exchange_strong(ptr, incr))
                        return true;
                }
                else
                    break;
            }

            return false;
        }

        void remove_reference()
        {
            pool_data *ptr = next.load();
            while (true)
            {
                if ((reinterpret_cast<unsigned long>(ptr) & 1) && (reinterpret_cast<unsigned long>(ptr) & ~((unsigned long)1)))
                {
                    auto *decr = reinterpret_cast<pool_data *>(reinterpret_cast<unsigned long>(ptr) - 0x10);
                    if (decr == reinterpret_cast<pool_data*>(1))
                        decr = 0;
                    if (next.compare_exchange_strong(ptr, decr))
                    {
                        if (decr == 0)
                            source->finish(this);
                        return;
                    }
                }
                else
                    return;
            }
        }
    };

    std::atomic<pool_data *> dat_;

    pool_ptr(pool_data *data) noexcept;
    friend class pool<TValue, TAlloc>;
public:
    /**
     * \brief Construct a null pool pointer
     */
    constexpr pool_ptr() noexcept :
            dat_(nullptr)
    { }

    /**
     * \brief Convenience wrapper to assign nullptr to pool_ptr instances
     */
    constexpr pool_ptr(nullptr_t ptr) noexcept :
            pool_ptr()
    { }

    /**
     * \brief Copy constructor
     */
    pool_ptr(const pool_ptr &cpy) noexcept;

    /**
     * \brief Move constructor
     */
    pool_ptr(pool_ptr &&mv) noexcept;

    /**
     * \brief destructor
     */
    ~pool_ptr();

    /**
     * \brief Assignment operator
     */
    pool_ptr &operator=(const pool_ptr &ptr) noexcept;

    /**
     * \brief Assignment move operator
     */
    pool_ptr &operator=(pool_ptr &&mv) noexcept;

    /**
     * \brief Dereference the pointer
     */
    TValue *operator->() noexcept;

    /**
     * \brief Dereference the pointer
     */
    TValue &operator*() noexcept;

    /**
     * Comparison operator. Compares the two ptrs (not values)
     */
    bool operator==(const pool_ptr &other) const noexcept;

    /**
     * Comparison operator. Compares the two ptrs (not values)
     */
    bool operator!=(const pool_ptr &other) const noexcept;

    /**
     * \brief bool operator - true if non-null
     */
    operator bool() const noexcept;

    /**
     * \brief Dereference the pointer
     */
    const TValue *operator->() const noexcept;

    /**
     * \brief Dereference the pointer
     */
    const TValue &operator*() const noexcept;

    /**
     * \brief Do an atomic compare and exchange with another pointer
     *
     * \param ptr the other pointer to compare and exchange
     * \param other the expected value of the pointer
     * \param order the memory order to use in the cmpxchg
     *
     * \return whether or not the compare/exchange succeeded
     */
    bool compare_exchange_strong(pool_ptr &ptr, const pool_ptr &other, std::memory_order order = std::memory_order_seq_cst) noexcept;

    /**
     * Do an atomic exchange where the value is set to nptr and the previous value is returned
     *
     * \param ndata the new pointer value to swap in
     *
     * \return the previous value of the ptr
     */
    pool_ptr exchange(const pool_ptr &nptr, std::memory_order order = std::memory_order_seq_cst) noexcept;
};

template<typename TValue, template<typename ...> class TAlloc>
class pool
{
    using data_type = typename pool_ptr<TValue, TAlloc>::pool_data;
    std::atomic<data_type *> v_;
    bool active_;
    TAlloc<data_type> a_;

    data_type *allocnew(const TValue &value)
    {
        data_type *data = a_.allocate(1);
        a_.construct(data, this, data->next = reinterpret_cast<data_type *>(0x11));

        new (&data->value()) TValue(value);
        return data;
    }

    void finish(data_type *data) noexcept
    {
        data->value().~TValue();
        if (!active_)
        {
            a_.deallocate(data, 1);
            return;
        }

#ifndef CXXMETRICS_DISABLE_POOLING
        data_type *head = v_.load();
        while (true)
        {
            if (!active_)
            {
                a_.deallocate(data, 1);
                return;
            }

            data->next = head;
            if (v_.compare_exchange_strong(head, data))
                break;
        }
#else
        a_.deallocate(data, 1);
#endif
    }

    friend class pool_ptr<TValue, TAlloc>;
public:
    /**
     * \brief default constructor
     */
    pool() noexcept :
            v_(nullptr),
            active_(true)
    {
    }

    /**
     * \brief destructor
     */
    ~pool()
    {
        active_ = false;

        while (v_)
        {
            auto tmp = v_.load();
            v_ = tmp->next.load();
            a_.deallocate(tmp, 1);
        }
    }

    /**
     * \brief Allocate a new object from the pool
     */
    pool_ptr<TValue, TAlloc> allocate(const TValue &value = TValue()) noexcept
    {
        data_type *head = v_.load();

        while (true)
        {
#ifndef CXXMETRICS_DISABLE_POOLING
            if (head == nullptr)
#endif
                return pool_ptr<TValue, TAlloc>(allocnew(value));

            data_type *next = head->next;
            if (v_.compare_exchange_strong(head, next))
            {
                head->next = reinterpret_cast<data_type *>(0x11);
                new (&head->value()) TValue(value);
                return pool_ptr<TValue, TAlloc>(head);
            }
        }
    }
};

template<typename TValue, template<typename ...> class TAlloc>
pool_ptr<TValue, TAlloc>::pool_ptr(pool_data *data) noexcept :
    dat_(nullptr)
{
    if (data && data->add_reference())
    {
        dat_ = data;
        data->remove_reference();
    }
}

template<typename TValue, template<typename ...> class TAlloc>
pool_ptr<TValue, TAlloc>::pool_ptr(const pool_ptr &copy) noexcept :
    dat_(nullptr)
{
    auto newdat = copy.dat_.load();
    if (newdat && newdat->add_reference())
        dat_ = newdat;
};

template<typename TValue, template<typename ...> class TAlloc>
pool_ptr<TValue, TAlloc>::pool_ptr(pool_ptr &&mv) noexcept :
        dat_(nullptr)
{
    pool_data *dat = mv.dat_.exchange(nullptr);
    dat_ = dat;
};

template<typename TValue, template<typename ...> class TAlloc>
pool_ptr<TValue, TAlloc>::~pool_ptr()
{
    auto dat = dat_.load();
    if (dat)
        dat->remove_reference();
};

template<typename TValue, template<typename ...> class TAlloc>
pool_ptr<TValue, TAlloc> &pool_ptr<TValue, TAlloc>::operator=(const pool_ptr &cp) noexcept
{
    auto ndat = cp.dat_.load();
    if (ndat && !ndat->add_reference())
        ndat = nullptr;

    auto odat = dat_.exchange(ndat);
    if (odat)
        odat->remove_reference();

    return *this;
};

template<typename TValue, template<typename ...> class TAlloc>
pool_ptr<TValue, TAlloc> &pool_ptr<TValue, TAlloc>::operator=(pool_ptr &&ptr) noexcept
{
    auto ndat = ptr.dat_.exchange(nullptr);
    if (ndat && !ndat->add_reference())
        ndat = nullptr;

    auto odat = dat_.exchange(ndat);
    if (odat)
        odat->remove_reference();
    if (ndat)
        ndat->remove_reference();

    return *this;
};

template<typename TValue, template<typename ...> class TAlloc>
TValue *pool_ptr<TValue, TAlloc>::operator->() noexcept
{
    return &dat_.load()->value();
};

template<typename TValue, template<typename ...> class TAlloc>
const TValue *pool_ptr<TValue, TAlloc>::operator->() const noexcept
{
    return &dat_.load()->value();
};

template<typename TValue, template<typename ...> class TAlloc>
TValue &pool_ptr<TValue, TAlloc>::operator*() noexcept
{
    return dat_.load()->value();
};

template<typename TValue, template<typename ...> class TAlloc>
pool_ptr<TValue, TAlloc>::operator bool() const noexcept
{
    return dat_.load() != nullptr;
};

template<typename TValue, template<typename ...> class TAlloc>
const TValue &pool_ptr<TValue, TAlloc>::operator*() const noexcept
{
    return dat_.load()->value();
};

template<typename TValue, template<typename ...> class TAlloc>
bool pool_ptr<TValue, TAlloc>::operator==(const pool_ptr &other) const noexcept
{
    return dat_.load() == other.dat_.load();
}

template<typename TValue, template<typename ...> class TAlloc>
bool pool_ptr<TValue, TAlloc>::operator!=(const pool_ptr &other) const noexcept
{
    return dat_.load() != other.dat_.load();
}

template<typename TValue, template<typename ...> class TAlloc>
bool pool_ptr<TValue, TAlloc>::compare_exchange_strong(pool_ptr &ptr, const pool_ptr &other, std::memory_order order) noexcept
{
    auto origvalue = ptr.dat_.load();
    auto oldvalue = origvalue;
    auto newvalue = other.dat_.load();

    if (newvalue && !newvalue->add_reference())
        newvalue = nullptr;

    if (dat_.compare_exchange_strong(oldvalue, newvalue, order))
    {
        if (newvalue == oldvalue)
        {
            newvalue->remove_reference();
            return true;
        }

        if (oldvalue)
            oldvalue->remove_reference();

        return true;
    }

    // oldvalue isn't the same value it was before
    if (oldvalue != origvalue)
        oldvalue->add_reference();
    if (newvalue)
        newvalue->remove_reference();

    ptr = pool_ptr(oldvalue);
    return false;
}

template<typename TValue, template<typename ...> class TAlloc>
pool_ptr<TValue, TAlloc> pool_ptr<TValue, TAlloc>::exchange(const pool_ptr &nptr, std::memory_order order) noexcept
{
    auto newvalue = nptr.dat_.load();
    if (newvalue && !newvalue->add_reference())
        newvalue = nullptr;
    auto oldvalue = dat_.exchange(newvalue, order);
    return pool_ptr(oldvalue);
}
}

}
#endif //CXXMETRICS_POOL_HPP
