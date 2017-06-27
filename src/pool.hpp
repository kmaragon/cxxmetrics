#ifndef CXXMETRICS_POOL_HPP
#define CXXMETRICS_POOL_HPP

namespace cxxmetrics
{

namespace internal
{

template<typename TValue, template<typename ...> class TAlloc = std::allocator>
class pool;

template<typename TValue>
struct _pool_data
{
    TValue value;
    struct _pool_data *next;
};

template<typename TValue>
class pool_ptr
{
    ::cxxmetrics::internal::_pool_data<TValue> *dat_;
public:
    pool_ptr(::cxxmetrics::internal::_pool_data<TValue> *data) noexcept :
            dat_(data)
    {}

    pool_ptr() noexcept :
        dat_(nullptr)
    { }

    pool_ptr(const pool_ptr &cp) = default;

    ~pool_ptr() = default;

    pool_ptr &operator=(const pool_ptr &cp) = default;

    TValue *operator->() noexcept
    {
        return &dat_->value;
    }

    TValue &operator*() noexcept
    {
        return dat_->value;
    }

    const TValue *operator->() const noexcept
    {
        return &dat_->value;
    }

    TValue &operator*() const noexcept
    {
        return dat_->value;
    }

    operator bool() const noexcept
    {
        return dat_ != nullptr;
    }

    ::cxxmetrics::internal::_pool_data<TValue> *ptr()
    {
        return dat_;
    }

    const ::cxxmetrics::internal::_pool_data<TValue> *ptr() const
    {
        return dat_;
    }
};

// a unique pointer from a pool that is returned to the pool when it goes out of scope
template<typename TValue, template<typename ...> class TAlloc = std::allocator>
class unique_pool_ptr
{
    ::cxxmetrics::internal::_pool_data<TValue> *dat_;
    pool<TValue, TAlloc> *pool_;
public:
    unique_pool_ptr() noexcept;

    unique_pool_ptr(::cxxmetrics::internal::_pool_data<TValue> *data, pool<TValue, TAlloc> *pool) noexcept;

    unique_pool_ptr(const unique_pool_ptr &cpy) = delete;

    unique_pool_ptr(unique_pool_ptr &&mv) noexcept;

    ~unique_pool_ptr();

    unique_pool_ptr &operator=(const unique_pool_ptr &ptr) = delete;

    unique_pool_ptr &operator=(unique_pool_ptr &&mv) noexcept;

    pool_ptr<TValue> release() noexcept;

    TValue *operator->() noexcept;

    TValue &operator*() noexcept;

    operator bool() const noexcept;

    const TValue *operator->() const noexcept;

    const TValue &operator*() const noexcept;

    const void *ptr() const noexcept;
};

template<typename TValue, template<typename ...> class TAlloc>
class pool
{
    using data_type = ::cxxmetrics::internal::_pool_data<TValue>;
    std::atomic<data_type *> v_;
    TAlloc<data_type> a_;

    data_type *allocnew()
    {
        data_type *data = a_.allocate(sizeof(data_type));
        new(&data->value) TValue();

        return data;
    }

public:
    pool() noexcept :
            v_(nullptr) {}

    ~pool()
    {
        auto v = v_.load();
        while (v)
        {
            auto tmp = v;
            v = v->next;

            tmp->value.~TValue();
            a_.deallocate(tmp, sizeof(TValue));
        }
    }

    void finish(const pool_ptr<TValue> &val) noexcept
    {
        data_type *head = v_.load();

        while (true)
        {
            ((data_type *)val.ptr())->next = head;
            if (v_.compare_exchange_strong(head, (data_type *)val.ptr()))
                break;
        }
    }

    void hard_delete(const pool_ptr<TValue> &val) noexcept
    {
        data_type *tmp = (data_type *)val.ptr();
        tmp->value.~TValue();
        a_.deallocate(tmp, sizeof(TValue));
    }

    unique_pool_ptr<TValue, TAlloc> get() noexcept
    {
        data_type *head = v_.load();

        while (true)
        {
            if (head == nullptr)
                return unique_pool_ptr<TValue, TAlloc>(allocnew(), this);

            data_type *next = head->next;
            if (v_.compare_exchange_strong(head, next))
                return unique_pool_ptr<TValue, TAlloc>(head, this);
        }
    }
};

template<typename TValue, template<typename ...> class TAlloc>
unique_pool_ptr<TValue, TAlloc>::unique_pool_ptr() noexcept :
        dat_(nullptr),
        pool_(nullptr)
{
}

template<typename TValue, template<typename ...> class TAlloc>
unique_pool_ptr<TValue, TAlloc>::unique_pool_ptr(::cxxmetrics::internal::_pool_data<TValue> *data, pool<TValue, TAlloc> *p) noexcept :
        dat_(data),
        pool_(p)
{
}

template<typename TValue, template<typename ...> class TAlloc>
unique_pool_ptr<TValue, TAlloc>::unique_pool_ptr(unique_pool_ptr &&mv) noexcept :
        dat_(mv.dat_),
        pool_(mv.pool_)
{
    mv.dat_ = nullptr;
};

template<typename TValue, template<typename ...> class TAlloc>
unique_pool_ptr<TValue, TAlloc>::~unique_pool_ptr()
{
    if (dat_ != nullptr)
        pool_->finish(pool_ptr<TValue>(dat_));
};

template<typename TValue, template<typename ...> class TAlloc>
unique_pool_ptr<TValue, TAlloc> &unique_pool_ptr<TValue, TAlloc>::operator=(unique_pool_ptr &&ptr) noexcept
{
    if (dat_ != nullptr)
        pool_->finish(dat_);

    dat_ = ptr.release().ptr();
    pool_ = ptr.pool_;

    return *this;
};

template<typename TValue, template<typename ...> class TAlloc>
pool_ptr<TValue> unique_pool_ptr<TValue, TAlloc>::release() noexcept
{
    auto ptr = dat_;
    dat_ = nullptr;
    return ptr;
};

template<typename TValue, template<typename ...> class TAlloc>
TValue *unique_pool_ptr<TValue, TAlloc>::operator->() noexcept
{
    return &dat_->value;
};

template<typename TValue, template<typename ...> class TAlloc>
const TValue *unique_pool_ptr<TValue, TAlloc>::operator->() const noexcept
{
    return &dat_->value;
};

template<typename TValue, template<typename ...> class TAlloc>
TValue &unique_pool_ptr<TValue, TAlloc>::operator*() noexcept
{
    return dat_->value;
};

template<typename TValue, template<typename ...> class TAlloc>
unique_pool_ptr<TValue, TAlloc>::operator bool() const noexcept
{
    return dat_ != nullptr;
};

template<typename TValue, template<typename ...> class TAlloc>
const TValue &unique_pool_ptr<TValue, TAlloc>::operator*() const noexcept
{
    return dat_->value;
};

template<typename TValue, template<typename ...> class TAlloc>
const void *unique_pool_ptr<TValue, TAlloc>::ptr() const noexcept
{
    return dat_;
}

}

}
#endif //CXXMETRICS_POOL_HPP
