#ifndef CXXMETRICS_SLIDING_WINDOW_HPP
#define CXXMETRICS_SLIDING_WINDOW_HPP

#include "ewma.hpp"
#include "ringbuf.hpp"

namespace cxxmetrics
{

namespace internal
{

template<typename T, typename TClockGet>
class timed_data
{

public:
    using clock_point = typename clock_traits<TClockGet>::clock_point;

    timed_data() noexcept;

    timed_data(const TClockGet &t) noexcept;

    timed_data(const T &val, const TClockGet &t = TClockGet()) noexcept;

    timed_data(const timed_data &) noexcept = default;

    timed_data &operator=(const timed_data &) noexcept = default;

    bool operator<(const timed_data &other) const noexcept;

    bool operator<=(const timed_data &other) const noexcept;

    bool operator>(const timed_data &other) const noexcept;

    bool operator>=(const timed_data &other) const noexcept;

    bool operator==(const timed_data &other) const noexcept;

    bool operator!=(const timed_data &other) const noexcept;

    inline clock_point time() const
    {
        return time_;
    }

    inline T value() const
    {
        return value_;
    }

private:
    clock_point time_;
    T value_;
};

template<typename T, typename TClockGet>
timed_data<T, TClockGet>::timed_data() noexcept
{
    // invalid state constructor
}

template<typename T, typename TClockGet>
timed_data<T, TClockGet>::timed_data(const TClockGet &clock) noexcept :
        time_(clock())
{
}

template<typename T, typename TClockGet>
timed_data<T, TClockGet>::timed_data(const T &val, const TClockGet &clock) noexcept :
        time_(clock()),
        value_(val)
{
}

template<typename T, typename TClockGet>
bool timed_data<T, TClockGet>::operator<(const timed_data &other) const noexcept
{
    if (value_ < other.value_)
        return true;
    if (value_ == other.value_)
        return time_ < other.time_;
    return false;
};

template<typename T, typename TClockGet>
bool timed_data<T, TClockGet>::operator<=(const timed_data &other) const noexcept
{
    if (value_ < other.value_)
        return true;
    if (value_ == other.value_)
        return time_ <= other.time_;
    return false;
};

template<typename T, typename TClockGet>
bool timed_data<T, TClockGet>::operator>(const timed_data &other) const noexcept
{
    if (value_ > other.value_)
        return true;
    if (value_ == other.value_)
        return time_ > other.time_;
    return false;
};

template<typename T, typename TClockGet>
bool timed_data<T, TClockGet>::operator>=(const timed_data &other) const noexcept
{
    if (value_ > other.value_)
        return true;
    if (value_ == other.value_)
        return time_ >= other.time_;
    return false;
};

template<typename T, typename TClockGet>
bool timed_data<T, TClockGet>::operator==(const timed_data &other) const noexcept
{
    return value_ == other.value_ && time_ == other.time_;
};

template<typename T, typename TClockGet>
bool timed_data<T, TClockGet>::operator!=(const timed_data &other) const noexcept
{
    return value_ != other.value_ || time_ != other.time_;
};

}

/**
 * \brief A reservoir implementation that manages samples based on activity from the most recent time period
 *
 * \tparam TElem the type of element in the reservoir
 * \tparam TMaxSize the maximum size of the data in the reservoir
 * \tparam TClockGet the 'functor' (the C++ kind, not an actual functor) that gets the current time
 */
template<typename TElem, size_t TMaxSize, typename TClockGet = steady_clock_point>
class sliding_window_reservoir
{
public:
    /**
     * \brief the type of data that the sliding window is represented in based on the clock 'functor'
     */
    using window_type = typename internal::clock_traits<TClockGet>::clock_diff;
    using value_type = TElem;
private:

    class transform_iterator : public std::iterator<TElem, std::input_iterator_tag>
    {
        typename internal::ringbuf<internal::timed_data<TElem, TClockGet>, TMaxSize>::iterator it_;
    public:
        transform_iterator() = default;

        transform_iterator(
                const typename internal::ringbuf<internal::timed_data<TElem, TClockGet>, TMaxSize>::iterator &real) noexcept
                : it_(real) {}

        transform_iterator(const transform_iterator &other) = default;

        ~transform_iterator() = default;

        transform_iterator &operator=(const transform_iterator &other) = default;

        bool operator==(const transform_iterator &other) const noexcept { return it_ == other.it_; }

        bool operator!=(const transform_iterator &other) const noexcept { return it_ != other.it_; }

        transform_iterator &operator++() noexcept { ++it_; return *this; }

        TElem operator*() const noexcept { return it_->value(); }
    };

    TClockGet clock_;
    window_type window_;
    internal::ringbuf<internal::timed_data<TElem, TClockGet>, TMaxSize> data_;

public:
    /**
     * \brief Construct a sliding window reservoir
     *
     * \param window the size of the sliding window over which the reservoir tracks
     * \param clock the clock object to use for deriving timestamps
     */
    explicit sliding_window_reservoir(const window_type &window = time::minutes(1), const TClockGet &clock = TClockGet()) noexcept;

    /**
     * \brief Copy constructor
     */
    sliding_window_reservoir(const sliding_window_reservoir &other) noexcept;

    ~sliding_window_reservoir() = default;

    /**
     * \brief Assignment operator
     */
    sliding_window_reservoir &operator=(const sliding_window_reservoir &other) noexcept;

    /**
     * \brief Update the sliding window reservoir with a value (using the clock in the template parameter)
     */
    void update(const TElem &v) noexcept;

    /**
     * \brief Get a snapshot of the reservoir
     *
     * \return a reservoir snapshot
     */
    reservoir_snapshot snapshot() const noexcept;
};

template<typename TElem, size_t TMaxSize, typename TClockGet>
sliding_window_reservoir<TElem, TMaxSize, TClockGet>::sliding_window_reservoir(const window_type &window,
                                                                               const TClockGet &clock) noexcept :
        clock_(clock),
        window_(window)
{
}

template<typename TElem, size_t TMaxSize, typename TClockGet>
sliding_window_reservoir<TElem, TMaxSize, TClockGet>::sliding_window_reservoir(
        const sliding_window_reservoir &other) noexcept :
        clock_(other.clock_),
        window_(other.window_),
        data_(other.data_)
{
}

template<typename TElem, size_t TMaxSize, typename TClockGet>
sliding_window_reservoir<TElem, TMaxSize, TClockGet> &
sliding_window_reservoir<TElem, TMaxSize, TClockGet>::operator=(const sliding_window_reservoir &other) noexcept
{
    data_ = other.data_;
    clock_ = other.clock_;
    window_ = other.window_;
}

template<typename TElem, size_t TMaxSize, typename TClockGet>
void sliding_window_reservoir<TElem, TMaxSize, TClockGet>::update(const TElem &v) noexcept
{
    // set up our values for trimming old stuff out
    auto now = clock_();
    auto min = now - window_;

    auto cond = [&min](const auto &timedval) {
        if (timedval.time() < min)
            return true;
        return false;
    };

    // trim our older values
    while (data_.shift_if(cond));

    data_.push(internal::timed_data<TElem, TClockGet>(v, clock_));
}

template<typename TElem, size_t TMaxSize, typename TClockGet>
reservoir_snapshot sliding_window_reservoir<TElem, TMaxSize, TClockGet>::snapshot() const noexcept
{
    auto now = clock_();
    auto min = now - window_;

    auto begin = data_.begin();
    for (; begin != data_.end(); ++begin)
    {
        if (begin->time() >= min)
            break;
    }

    return reservoir_snapshot(transform_iterator(begin), transform_iterator(data_.end()), TMaxSize);
}

}

#endif //CXXMETRICS_SLIDING_WINDOW_HPP
