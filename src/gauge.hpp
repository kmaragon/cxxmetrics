#ifndef CXXMETRICS_GAUGE_HPP
#define CXXMETRICS_GAUGE_HPP

#include "metric.hpp"

namespace cxxmetrics
{


namespace gauges
{

/**
 * \brief The generic type for a gauge
 *
 * \tparam TGaugeValueType The type of data that the gauge produces
 */
template<typename TGaugeValueType>
class gauge
{
public:
    virtual TGaugeValueType get() noexcept = 0;
    virtual TGaugeValueType get() const noexcept = 0;
};

template<typename TGaugeType>
class primitive_gauge : gauge<TGaugeType>
{
    TGaugeType value_;
public:
    primitive_gauge(const TGaugeType &initial_value = TGaugeType()) noexcept;
    primitive_gauge(const primitive_gauge &copy) noexcept = default;
    primitive_gauge(primitive_gauge &&mv) noexcept = default;
    ~primitive_gauge() = default;

    primitive_gauge &operator=(const TGaugeType &value) noexcept;
    primitive_gauge &operator=(const primitive_gauge &other) noexcept = default;
    primitive_gauge &operator=(primitive_gauge &&other) noexcept = default;

    void set(TGaugeType value) noexcept;

    TGaugeType get() noexcept override;
    TGaugeType get() const noexcept override;
};

template<typename TGaugeType>
primitive_gauge<TGaugeType>::primitive_gauge(const TGaugeType &initial_value) noexcept :
    value_(initial_value)
{ }

template<typename TGaugeType>
primitive_gauge<TGaugeType> &primitive_gauge<TGaugeType>::operator=(const TGaugeType &value) noexcept
{
    value_ = value;
    return *this;
}

template<typename TGaugeType>
void primitive_gauge<TGaugeType>::set(TGaugeType value) noexcept
{
    value_ = value;
}

template<typename TGaugeType>
TGaugeType primitive_gauge<TGaugeType>::get() noexcept
{
    return value_;
}

template<typename TGaugeType>
TGaugeType primitive_gauge<TGaugeType>::get() const noexcept
{
    return value_;
}

template<typename TFn>
struct functional_gauge_info
{
private:
    static auto gtp()
    {
        TFn *f;
        return (*f)();
    }

public:
    using gauge_type = typename std::decay<decltype(functional_gauge_info<TFn>::gtp())>::type;
};

template<typename TFn>
class functional_gauge : public gauge<typename functional_gauge_info<TFn>::gauge_type>
{
    TFn fn_;
public:
    using gauge_type = typename functional_gauge_info<TFn>::gauge_type;

    functional_gauge(const TFn &fn) noexcept;
    functional_gauge(const functional_gauge &copy) noexcept = default;
    functional_gauge(functional_gauge &&mv) noexcept = default;

    ~functional_gauge() = default;

    functional_gauge &operator=(const functional_gauge &other) noexcept = default;
    functional_gauge &operator=(functional_gauge &&mv) noexcept = default;

    gauge_type get() noexcept override;
    gauge_type get() const noexcept override;
};

template<typename TFn>
functional_gauge<TFn>::functional_gauge(const TFn &fn) noexcept :
    fn_(fn)
{ }

template<typename TFn>
typename functional_gauge<TFn>::gauge_type functional_gauge<TFn>::get() noexcept
{
    return fn_();
}

template<typename TFn>
typename functional_gauge<TFn>::gauge_type functional_gauge<TFn>::get() const noexcept
{
    return fn_();
}

template<typename TRefType>
class referential_gauge : public gauge<TRefType>
{
    std::reference_wrapper<TRefType> value_;
public:
    referential_gauge(TRefType &t) noexcept;
    referential_gauge(const referential_gauge &copy) noexcept;
    ~referential_gauge() = default;

    referential_gauge &operator=(const referential_gauge &copy) noexcept;

    TRefType get() noexcept override;
    TRefType get() const noexcept override;
};

template<typename TRefType>
referential_gauge<TRefType>::referential_gauge(TRefType &t) noexcept :
    value_(t)
{ }

template<typename TRefType>
referential_gauge<TRefType>::referential_gauge(const referential_gauge &other) noexcept :
        value_(other.value_.get())
{ }

template<typename TRefType>
referential_gauge<TRefType> &referential_gauge<TRefType>::operator=(const referential_gauge &copy) noexcept
{
    value_ = copy.value_.get();
    return *this;
}

template<typename TRefType>
TRefType referential_gauge<TRefType>::get() noexcept
{
    return value_.get();
}

template<typename TRefType>
TRefType referential_gauge<TRefType>::get() const noexcept
{
    return value_.get();
}

}

/**
 * \brief A simple Gauge metric
 *
 * The TGaugeType has a heavy influence over the behavior of the gauge.
 *
 * If it is an std::function<ReturnType()>, the function is called to provide the underlying data. It is also move
 * constructable and assignable. But not settable.
 *
 * If it is a reference, or a pointer, the value to which the reference refers is used as the value. It is only
 * copy constructable and is not settable.
 *
 * If it's a simple assignable type, say int, or an std::string, it is copy constructable, move constructble and
 * settable. In that case, a set() function is available.
 *
 * No matter the type that the gauge is, there will always be a get() function that gets the value of the gauge
 *
 * \tparam TGaugeType The type of data for the gauge
 */
template<typename TGaugeType>
class gauge : public gauges::primitive_gauge<TGaugeType>, public metric<gauge<TGaugeType>>
{
public:
    explicit gauge(const TGaugeType &value = TGaugeType()) noexcept :
            gauges::primitive_gauge<TGaugeType>(value)
    { }
    gauge(const gauge &copy) noexcept = default;
    gauge(gauge &&mv) noexcept = default;

    ~gauge() = default;

    inline gauge &operator=(const TGaugeType &value) noexcept
    {
        gauges::primitive_gauge<TGaugeType>::operator=(value);
        return *this;
    }
    gauge &operator=(const gauge &other) noexcept = default;
    gauge &operator=(gauge &&mv) noexcept = default;
};

template<typename TFn>
class gauge<std::function<TFn()>> : public gauges::functional_gauge<std::function<TFn()>>, public metric<gauge<TFn()>>
{
public:
    explicit gauge(const std::function<TFn()> &fn) noexcept :
            gauges::functional_gauge<std::function<TFn()>>(fn)
    { }
    gauge(const gauge &copy) noexcept = default;
    gauge(gauge &&mv) noexcept = default;
    ~gauge() = default;

    gauge &operator=(const gauge &other) noexcept = default;
    gauge &operator=(gauge &&mv) noexcept = default;
};

template<typename TGaugeType>
class gauge<TGaugeType *> : public gauges::referential_gauge<TGaugeType>, public metric<gauge<TGaugeType &>>
{
public:
    inline explicit gauge(TGaugeType *ptr) noexcept :
            gauges::referential_gauge<TGaugeType>(*ptr)
    { }
    gauge(const gauge &copy) noexcept = default;
    ~gauge() = default;

    gauge &operator=(const gauge &other) noexcept = default;
};

template<typename TGaugeType>
class gauge<TGaugeType &> : public gauges::referential_gauge<TGaugeType>, public metric<gauge<TGaugeType &>>
{
public:
    inline explicit gauge(TGaugeType &ref) noexcept :
            gauges::referential_gauge<TGaugeType>(ref)
    { }
    gauge(const gauge &copy) noexcept = default;
    ~gauge() = default;

    gauge &operator=(const gauge &other) noexcept = default;
};

}

#endif //CXXMETRICS_GAUGE_HPP
