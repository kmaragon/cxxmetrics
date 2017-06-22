#ifndef CXXMETRICS_RINGBUF_HPP
#define CXXMETRICS_RINGBUF_HPP

#include <atomic>
#include <thread>

namespace cxxmetrics
{

/**
 * \brief A simple fixed-size ring buffer implementation because there still isn't one in C++17
 *
 * \tparam TElemType The type of element in the ring buffer.
 * \tparam TSize The size of the ring buffer
 */
template<typename TElemType, size_t TSize>
class ringbuf
{
    constexpr static int64_t limit_ = TSize + 1;
    TElemType data_[limit_];

    // where the start of the list is. If this is == tail, the
    // list is empty
    std::atomic_int_fast64_t head_;

    // which element is ready to use. This will always be
    // logically less than tail (so it might be the end of the
    // array while tail is the beginning)
    std::atomic_int_fast64_t ready_;

    // where the end of the list is.
    std::atomic_int_fast64_t tail_;

public:

    class iterator : public std::iterator<std::input_iterator_tag, TElemType>
    {
        int64_t offset_;
        int64_t headat_;
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
     * @param end The end of the iterator
     */
    template<typename TInputIterator>
    ringbuf(TInputIterator &start, const TInputIterator &end) noexcept;

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
     * \brief Shift the element from the front of the ring buffer
     */
    TElemType shift() noexcept;

    /**
     * \brief Conditionally (and atomically) shift the front of the ring buffer if the item satisfies a predicate
     *
     * There is no guarantee that the predicate will be called once. If it returns true and the item was already be
     * popped, it will be called again until the buffer is empty, the function returns false, or the item was removed
     *
     * \param fn the predicate to apply to the item (using operator() with the element as an argument returning bool) to determine whether to shift the buffer
     */
    template<typename TPredicate>
    bool shift_if(const TPredicate &fn) noexcept;

    /**
     * \brief
     * @param elem
     */
    void push(const TElemType &elem) noexcept;

    /**
     * \brief Get the size of the ring buffer
     *
     * @return the size of the ring buffer
     */
    size_t size() const noexcept;

private:
    inline static bool is_within(int64_t v, int64_t low, int64_t high)
    {
        // the order of low vs high matters here
        // if high is less than low, we wrap around
        if (high < low)
            return v >= low || v <= high;

        return v >= low && v <= high;
    }

    inline static bool are_neighbors(int64_t a, int64_t b)
    {
        if (std::abs(a - b) == 1)
            return true;

        if ((a == limit_ || b == limit_) && (a == 0 || b == 0))
            return true;

        return false;
    }
    friend class iterator;
};

template<typename TElemType, size_t TSize>
ringbuf<TElemType, TSize>::iterator::iterator() noexcept :
    buf_(nullptr),
    offset_(-1),
    headat_(0)
{
};

template<typename TElemType, size_t TSize>
ringbuf<TElemType, TSize>::iterator::iterator(const ringbuf *rb, int64_t offset) noexcept :
    buf_(rb)
{
    int64_t head = rb->head_.load();
    while (true)
    {
        int64_t tail = rb->tail_.load(std::memory_order_acquire);
        int64_t at = head + offset;
        if (head == tail)
        {
            at = -1;
            // The container is empty... just continue
            break;
        }

        current_ = rb->data_[at];
        if (rb->head_.load(std::memory_order_release) == head)
        {
            offset_ = at;
            headat_ = head;
            break;
        }
    }

}

template<typename TElemType, size_t TSize>
bool ringbuf<TElemType, TSize>::iterator::operator==(const iterator &other) const noexcept
{
    if (offset_ < 0)
        return other.offset_ < 0;

    if (other.buf_ != buf_)
        return false;

    return other.offset_ == offset_;
}

template<typename TElemType, size_t TSize>
bool ringbuf<TElemType, TSize>::iterator::operator!=(const iterator &other) const noexcept
{
    if (offset_ < 0)
        return other.offset_ >= 0;

    return (other.buf_ != buf_) || other.offset_ != offset_;
}

template<typename TElemType, size_t TSize>
typename ringbuf<TElemType, TSize>::iterator &ringbuf<TElemType, TSize>::iterator::operator++() noexcept
{
    if (offset_ < 0)
        return *this;

    while (true)
    {
        int64_t head = buf_->head_.load();

        int64_t ready = buf_->ready_.load();
        int64_t tail = buf_->tail_.load();
        int64_t offset = offset_;

        // first check to see if the head surpassed our current offset
        if ((head > headat_) && buf_->is_within(offset, headat_, head - 1))
        {
            // yep - our offset went past the head
            // so we'll jump the offset and keep going with our logic
            offset = head;
            current_ = buf_->data_[head];

        }
        else
        {
            offset = (offset + 1) % buf_->limit_;
            current_ = buf_->data_[offset];
        }

        // are we at the end?
        if (offset == tail)
        {
            offset_ = -1;
            return *this;
        }

        // first check and see if we are on a cell that is not yet ready
        if (buf_->is_within(offset, ready + 1, tail))
        {
            // we're in that awkward spot. We need to try again
            std::this_thread::yield();
            continue;
        }

        // ok - just make sure the head didn't jump since we started all this
        if (buf_->head_.load() != head)
        {
            // the head moved - we might have a problem
            continue;
        }

        offset_ = offset;
        return *this;
    }
}

template<typename TElemType, size_t TSize>
const TElemType *ringbuf<TElemType, TSize>::iterator::operator->() const noexcept
{
    return &current_;
}

template<typename TElemType, size_t TSize>
ringbuf<TElemType, TSize>::ringbuf() noexcept :
        head_(0),
        ready_(-1),
        tail_(0)
{
    static_assert(TSize > 1, "The ringbuffer must have a size of at least 2");
}

template<typename TElemType, size_t TSize>
ringbuf<TElemType, TSize>::ringbuf(const ringbuf &other) noexcept :
        head_(0),
        ready_(-1),
        tail_(0)
{
    for (auto i = other.begin(); i != other.end(); ++i)
        push(*i);
}

template<typename TElemType, size_t TSize>
ringbuf<TElemType, TSize> &ringbuf<TElemType, TSize>::operator=(const ringbuf &other) noexcept
{
    head_.store(0);
    tail_.store(0);
    ready_.store(-1);

    for (auto i = other.begin(); i != other.end(); ++i)
        push(*i);
}

template<typename TElemType, size_t TSize>
typename ringbuf<TElemType, TSize>::iterator ringbuf<TElemType, TSize>::begin() const noexcept
{
    return iterator(this, 0);
}

template<typename TElemType, size_t TSize>
typename ringbuf<TElemType, TSize>::iterator ringbuf<TElemType, TSize>::end() const noexcept
{
    return iterator();
}

template<typename TElemType, size_t TSize>
TElemType ringbuf<TElemType, TSize>::shift() noexcept
{
    TElemType value;
    shift_if([&value](const TElemType &v) {
        value = v;
        return true;
    });
    return value;
}

template<typename TElemType, size_t TSize>
template<typename TPredicate>
bool ringbuf<TElemType, TSize>::shift_if(const TPredicate &fn) noexcept
{
    int64_t start = head_.load();

    while (true)
    {
        int64_t ready = ready_.load();
        int64_t tail = tail_.load();
        int64_t head = start;

        // head is between ready and tail
        // we need to move it to the other side
        // of tail
        if (is_within(head, ready, tail) && !are_neighbors(ready, tail))
        {
            // This is the special case where either tail just
            // lapped head or we had an empty list that is now just being
            // inserted into. But it's not done yet.
            //
            // The problem here is that we don't know which of these cases
            // is happening now. The information to know is sitting on the stack
            // of the push thread(s). So we'll just need to try again.
            //
            // Then, either 1 of 2 things will happen: ready will catch
            // up and be equal to head_ or head_ will be moved to the other side
            // of tail
            std::this_thread::yield();
            start = head_.load();
            continue;
        }

        if (head == tail)
        {
            // This isn't a special case, we're just at the end
            // of the list.
            return false;
        }

        if (!fn(data_[head++]))
            return false;

        // we only finish if someone didn't change head out from under us
        // for example,
        if (head_.compare_exchange_strong(start, head))
            return true;
    }
}


template<typename TElemType, size_t TSize>
void ringbuf<TElemType, TSize>::push(const TElemType &elem) noexcept
{
    while (true)
    {
        int64_t tail = tail_.fetch_add(1);
        int64_t phead = head_.load();
        auto neededready = tail - 1;

        if (tail >= limit_)
        {
            // we fell over the end so we don't need to worry about the data being written here
            // but someone else might have fallen over before or after us. So we'll loop back around
            // and the first one to zero wins and everyone else will try again as such, we set it to
            // 1. And if this thread wins, it gets the zero slot
            tail++;
            if (!tail_.compare_exchange_strong(tail, 1))
                continue;

            tail = 0;
            neededready = limit_ - 1;
        }

        // we got the 'next' tail
        data_[tail] = elem;

        // we don't want to increment the ready counter
        // until it's all caught up. This ensures that we
        // notify the head that we're ready only when everyone
        // before us is also ready. We all have our reserved spots via
        // the atomic synchronization on tail
        while (ready_.load() != neededready)
            std::this_thread::yield();

        // If two pushes are happening at the same time, they
        // end up synchronizing here. All other threads are waiting
        // for their turn to bump ready.
        //
        // That makes this a good time to check to see if we just lapped
        // head. If so, head will be exactly tail because our previous thread
        // in line already bumped head to its incremented tail. So now it's our turn.
        // This will keep happening until in the end, it settles where:
        // head_ = tail_ + 1
        // ready_ = tail_ - 1
        //
        // in the meantime, it's possible that:
        // ready_ <= head_ <= tail_ so we need to account for that
        // when messing with head too.
        //
        // The specific check is to see if head was basically equal to our
        // *new* tail. so [___TH__] -> [____B__] (where B means both)
        // which requires [____TH_] in the end
        neededready = (tail + 1) % limit_;
        head_.compare_exchange_strong(neededready, (tail + 2) % limit_);
        ready_.store(tail);

        break;
    }
}

template<typename TElemType, size_t TSize>
size_t ringbuf<TElemType, TSize>::size() const noexcept
{
    int64_t head = head_.load();
    int64_t ready = ready_.load() + 1; // ready is the last item available - not the length

    if (head > ready)
        return (limit_ - head) + ready;

    return ready - head;
};

}

#endif //CXXMETRICS_RINGBUF_HPP
