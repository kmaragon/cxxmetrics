#ifndef CXXMETRICS_CIRCULAR_RESERVOIR_HPP
#define CXXMETRICS_CIRCULAR_RESERVOIR_HPP

/**
 * \brief A type of reservoir that keeps elements in precise order in a circular buffer
 *
 * This can be used as is for a super simple non-uniform distribution. But more reasonably, it serves
 * a foundation for implementations of sliding window and exponentially decaying reservoirs
 */
template<typename TElem, int64_T TSize>
class simple_reservoir
{
    std::array<TElem, TSize> elems_;
    std::atomic_int_fast64_t head_;
    std::atomic_int_fast64_t tail_;

public:

    bool update_if(const TElem &value, const std::function<bool(const TElem &last)> &last);

    inline void update(const TElem &value)
    {
        update_if(value, [](const TElem &) { return true; });
    }

    bool shift_if(const std::function<bool(const TElem &)> &fn);
};

#endif //CXXMETRICS_CIRCULAR_RESERVOIR_HPP
