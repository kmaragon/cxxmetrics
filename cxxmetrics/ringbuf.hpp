#ifndef CXXMETRICS_RINGBUF_HPP
#define CXXMETRICS_RINGBUF_HPP

#include <atomic>
#include <thread>

namespace cxxmetrics
{

namespace internal
{

/**
 * \brief A simple fixed-size ring buffer implementation
 *
 * This is not a proper queue with expected queue semantics. It only guarantees consistency of "length".
 * So when iterating, there's no guarantee of order, because the writer thread can surpass the reader
 * thread and the reader will just keep reading the new items. It's meant to be used in reservoirs
 *
 * \tparam TElemType The type of element in the ring buffer.
 * \tparam TSize The size of the ring buffer
 */
template<typename TElemType, size_t TSize>
class ringbuf
{
    std::array<std::atomic<TElemType>, TSize> data_;

    // where the end of the list is.
    std::atomic_uint_fast64_t tail_;

    // how many elements in the ring buffer that are available
    std::atomic_uint_fast64_t size_;
public:

    class iterator : public std::iterator<std::input_iterator_tag, TElemType>
    {
        int64_t offset_;
        std::size_t remaining_;
        TElemType current_;
        const ringbuf *buf_;
    public:
        using value_type = TElemType;

        iterator() noexcept;

        iterator(const ringbuf *rb, int64_t offset = 0) noexcept;

        iterator(const iterator &it) noexcept = default;

        iterator &operator=(const iterator &other) noexcept = default;

        bool operator==(const iterator &other) const noexcept;

        bool operator!=(const iterator &other) const noexcept;

        iterator &operator++() noexcept;

        const TElemType *operator->() const noexcept;

        inline TElemType operator*() const noexcept
        {
            return current_;
        }

    };

    /**
     * \brief The default constructor
     */
    ringbuf() noexcept;

    /**
     * \brief Construct a ringbuf using an input iterator
     *
     * \tparam TInputIterator The type of iterator
     * \param start The start of the iterator
     * \param end The end of the iterator
     */
    template<typename TInputIterator>
    ringbuf(TInputIterator start, const TInputIterator &end) noexcept;

    /**
     * \brief Copy constructor
     */
    ringbuf(const ringbuf &other) noexcept;

    ~ringbuf() = default;

    /**
     * \brief Copy assignment operator
     */
    ringbuf &operator=(const ringbuf &other) noexcept;

    /**
     * \brief Get an iterator to the ring buffer
     */
    iterator begin() const noexcept;

    /**
     * \brief Get an iterator to the end of the ring buffer
     */
    iterator end() const noexcept;

    /**
     * \brief
     * \param elem
     */
    void push(const TElemType &elem) noexcept;

    /**
     * \brief Get the size of the ring buffer
     *
     * \return the size of the ring buffer
     */
    size_t size() const noexcept;

private:

    friend class iterator;
};

template<typename TElemType, size_t TSize>
ringbuf<TElemType, TSize>::iterator::iterator() noexcept :
        offset_(-1),
        remaining_(0),
        current_{},
        buf_(nullptr)
{
};

template<typename TElemType, size_t TSize>
ringbuf<TElemType, TSize>::iterator::iterator(const ringbuf *rb, int64_t offset) noexcept :
        offset_(offset),
        remaining_(0),
        current_{},
        buf_(rb)
{
    remaining_ = buf_->size_.load();
    if (remaining_ && buf_)
        current_ = buf_->data_[offset_++];
}

template<typename TElemType, size_t TSize>
bool ringbuf<TElemType, TSize>::iterator::operator==(const iterator &other) const noexcept
{
    if (remaining_ == 0)
        return other.remaining_ == 0;

    if (buf_ != other.buf_)
        return false;

    return other.offset_ == offset_;
}

template<typename TElemType, size_t TSize>
bool ringbuf<TElemType, TSize>::iterator::operator!=(const iterator &other) const noexcept
{
    return !operator==(other);
}

template<typename TElemType, size_t TSize>
typename ringbuf<TElemType, TSize>::iterator &ringbuf<TElemType, TSize>::iterator::operator++() noexcept
{
    if (!remaining_)
        return *this;

    current_ = buf_->data_[(offset_++) % TSize];
    remaining_--;

    return *this;
}

template<typename TElemType, size_t TSize>
const TElemType *ringbuf<TElemType, TSize>::iterator::operator->() const noexcept
{
    return &current_;
}

template<typename TElemType, size_t TSize>
ringbuf<TElemType, TSize>::ringbuf() noexcept :
        tail_(0),
        size_(0)
{
    static_assert(TSize > 1, "The ringbuffer must have a size of at least 2");
}

template<typename TElemType, size_t TSize>
template<typename TInputIterator>
ringbuf<TElemType, TSize>::ringbuf(TInputIterator start, const TInputIterator &end) noexcept :
        tail_(0),
        size_(0)
{
    std::size_t i = 0;
    for (; start != end && i < TSize; ++i, ++start)
        data_[i] = *start;

    tail_ = i;
}


template<typename TElemType, size_t TSize>
ringbuf<TElemType, TSize>::ringbuf(const ringbuf &other) noexcept :
        ringbuf(other.begin(), other.end())
{
}

template<typename TElemType, size_t TSize>
ringbuf<TElemType, TSize> &ringbuf<TElemType, TSize>::operator=(const ringbuf &other) noexcept
{
    tail_ = 0;
    size_ = 0;

    auto it = other.begin();
    std::size_t i = 0;
    for (; it != other.end() && i < TSize; ++i, ++it)
        data_[i] = *it;

    tail_ = i;
}

template<typename TElemType, size_t TSize>
typename ringbuf<TElemType, TSize>::iterator ringbuf<TElemType, TSize>::begin() const noexcept
{
    auto size = size_.load();
    if (size < TSize)
        return iterator(this, 0);

    return iterator(this, (tail_.load() + 1) % TSize);
}

template<typename TElemType, size_t TSize>
typename ringbuf<TElemType, TSize>::iterator ringbuf<TElemType, TSize>::end() const noexcept
{
    return iterator();
}

template<typename TElemType, size_t TSize>
void ringbuf<TElemType, TSize>::push(const TElemType &elem) noexcept
{
    auto writeloc = tail_.fetch_add(1) + 1;
    data_[writeloc % TSize] = elem;

    if (++writeloc > TSize)
        writeloc = TSize;

    auto csize = size_.load(std::memory_order_acquire);
    while (true)
    {
        if (writeloc <= csize || csize >= TSize)
            return;

        if (size_.compare_exchange_weak(csize, writeloc, std::memory_order_release, std::memory_order_acquire))
            return;
    }
}

template<typename TElemType, size_t TSize>
size_t ringbuf<TElemType, TSize>::size() const noexcept
{
    return size_.load();
};

}

}

#endif //CXXMETRICS_RINGBUF_HPP
