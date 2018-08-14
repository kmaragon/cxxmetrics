#ifndef CXXMETRICS_GAUGE_HPP
#define CXXMETRICS_GAUGE_HPP

#include "metric.hpp"
#include <type_traits>

namespace cxxmetrics
{

namespace gauges
{

enum gauge_aggregation_type
{
    aggregation_sum,
    aggregation_average
};

/**
 * \brief The generic type for a gauge
 *
 * \tparam TGaugeValueType The type of data that the gauge produces
 */
template<typename TGaugeValueType, gauge_aggregation_type TAggregation = aggregation_average>
class gauge
{
public:
    using snapshot_type = typename std::conditional<TAggregation == aggregation_sum, cumulative_value_snapshot, average_value_snapshot>::type;

    virtual TGaugeValueType get() noexcept = 0;
    virtual TGaugeValueType get() const noexcept = 0;
    snapshot_type make_snapshot() const noexcept
    {
        return snapshot_type(this->get());
    }
};

template<typename TGaugeType, gauge_aggregation_type TAggregation = aggregation_average>
class primitive_gauge : gauge<TGaugeType, TAggregation>
{
    TGaugeType value_;
public:
    primitive_gauge(const TGaugeType& initial_value = TGaugeType()) noexcept;
    primitive_gauge(const primitive_gauge& copy) = default;
    primitive_gauge(primitive_gauge&& mv) :
            value_(std::forward<TGaugeType>(mv.value_))
    { }
    ~primitive_gauge() = default;

    primitive_gauge& operator=(const TGaugeType& value) noexcept;
    primitive_gauge& operator=(const primitive_gauge& other) = default;

    primitive_gauge& operator=(primitive_gauge&& other) {
        value_ = std::move(other.value_);
        return *this;
    }

    void set(TGaugeType value) noexcept;

    TGaugeType get() noexcept override;
    TGaugeType get() const noexcept override;

    auto snapshot() const noexcept { return this->make_snapshot(); }
};

template<typename TGaugeType, gauge_aggregation_type TAggregation>
primitive_gauge<TGaugeType, TAggregation>::primitive_gauge(const TGaugeType& initial_value) noexcept :
    value_(initial_value)
{ }

template<typename TGaugeType, gauge_aggregation_type TAggregation>
primitive_gauge<TGaugeType, TAggregation> &primitive_gauge<TGaugeType, TAggregation>::operator=(const TGaugeType& value) noexcept
{
    value_ = value;
    return *this;
}

template<typename TGaugeType, gauge_aggregation_type TAggregation>
void primitive_gauge<TGaugeType, TAggregation>::set(TGaugeType value) noexcept
{
    value_ = value;
}

template<typename TGaugeType, gauge_aggregation_type TAggregation>
TGaugeType primitive_gauge<TGaugeType, TAggregation>::get() noexcept
{
    return value_;
}

template<typename TGaugeType, gauge_aggregation_type TAggregation>
TGaugeType primitive_gauge<TGaugeType, TAggregation>::get() const noexcept
{
    return value_;
}

template<typename TFn>
struct functional_gauge_info
{
public:
    using gauge_type = typename std::decay<decltype(std::declval<TFn>()())>::type;
};

template<typename TFn, gauge_aggregation_type TAggregation = aggregation_average>
class functional_gauge : public gauge<typename functional_gauge_info<TFn>::gauge_type, TAggregation>
{
    TFn fn_;
public:
    using gauge_type = typename functional_gauge_info<TFn>::gauge_type;

    functional_gauge(const TFn& fn) noexcept;
    functional_gauge(const functional_gauge& copy) = default;
    functional_gauge(functional_gauge&& mv) :
            fn_(std::forward<TFn>(mv.fn_))
    { }

    ~functional_gauge() = default;

    functional_gauge& operator=(const functional_gauge& other) = default;
    functional_gauge& operator=(functional_gauge&& mv)
    {
        fn_ = std::move(mv.fn_);
        return *this;
    }

    gauge_type get() noexcept override;
    gauge_type get() const noexcept override;

    value_snapshot snapshot() const noexcept { return this->make_snapshot(); }
};

template<typename TFn, gauge_aggregation_type TAggregation>
functional_gauge<TFn, TAggregation>::functional_gauge(const TFn& fn) noexcept :
    fn_(fn)
{ }

template<typename TFn, gauge_aggregation_type TAggregation>
typename functional_gauge<TFn, TAggregation>::gauge_type functional_gauge<TFn, TAggregation>::get() noexcept
{
    return fn_();
}

template<typename TFn, gauge_aggregation_type TAggregation>
typename functional_gauge<TFn, TAggregation>::gauge_type functional_gauge<TFn, TAggregation>::get() const noexcept
{
    return fn_();
}

template<typename TRefType, gauge_aggregation_type TAggregation = aggregation_average>
class referential_gauge : public gauge<TRefType>
{
    std::reference_wrapper<TRefType> value_;
public:
    referential_gauge(TRefType& t) noexcept;
    referential_gauge(const referential_gauge& copy) noexcept;
    ~referential_gauge() = default;

    referential_gauge& operator=(const referential_gauge& copy) noexcept;

    TRefType get() noexcept override;
    TRefType get() const noexcept override;

    auto snapshot() const noexcept { return this->make_snapshot(); }
};

template<typename TRefType, gauge_aggregation_type TAggregation>
referential_gauge<TRefType, TAggregation>::referential_gauge(TRefType& t) noexcept :
    value_(t)
{ }

template<typename TRefType, gauge_aggregation_type TAggregation>
referential_gauge<TRefType, TAggregation>::referential_gauge(const referential_gauge& other) noexcept :
        value_(other.value_.get())
{ }

template<typename TRefType, gauge_aggregation_type TAggregation>
referential_gauge<TRefType, TAggregation> &referential_gauge<TRefType, TAggregation>::operator=(const referential_gauge& copy) noexcept
{
    value_ = copy.value_.get();
    return *this;
}

template<typename TRefType, gauge_aggregation_type TAggregation>
TRefType referential_gauge<TRefType, TAggregation>::get() noexcept
{
    return value_.get();
}

template<typename TRefType, gauge_aggregation_type TAggregation>
TRefType referential_gauge<TRefType, TAggregation>::get() const noexcept
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
template<typename TGaugeType, gauges::gauge_aggregation_type TAggregation = gauges::aggregation_average>
class gauge : public gauges::primitive_gauge<TGaugeType, TAggregation>, public metric<gauge<TGaugeType>>
{
public:
    explicit gauge(const TGaugeType& value = TGaugeType()) noexcept :
            gauges::primitive_gauge<TGaugeType>(value)
    { }
    gauge(const gauge& copy) = default;
    gauge(gauge&& mv) = default;

    ~gauge() = default;

    gauge& operator=(const TGaugeType& value) noexcept
    {
        gauges::primitive_gauge<TGaugeType>::operator=(value);
        return *this;
    }
    gauge& operator=(const gauge& other) noexcept = default;
    gauge& operator=(gauge&& mv) noexcept = default;
};

template<typename T, gauges::gauge_aggregation_type TAggregation>
class gauge<std::function<T()>, TAggregation> : public gauges::functional_gauge<std::function<T()>>, public metric<gauge<std::function<T()>>>
{
public:
    explicit gauge(const std::function<T()> &fn) :
            gauges::functional_gauge<std::function<T()>>(fn)
    { }
    explicit gauge(std::function<T()> &&fn) noexcept :
            gauges::functional_gauge<std::function<T()>>(std::forward(fn))
    { }
    gauge(const gauge& copy) = default;
    gauge(gauge&& mv) = default;
    ~gauge() = default;

    gauge& operator=(const gauge& other) = default;
    gauge& operator=(gauge&& mv) = default;
};

template<typename TGaugeType, gauges::gauge_aggregation_type TAggregation>
class gauge<TGaugeType*, TAggregation> : public gauges::referential_gauge<TGaugeType>, public metric<gauge<TGaugeType &>>
{
public:
    inline explicit gauge(TGaugeType* ptr) noexcept :
            gauges::referential_gauge<TGaugeType>(*ptr)
    { }
    gauge(const gauge& copy) = default;
    ~gauge() = default;

    gauge& operator=(const gauge& other) = default;
};

template<typename TGaugeType, gauges::gauge_aggregation_type TAggregation>
class gauge<TGaugeType &, TAggregation> : public gauges::referential_gauge<TGaugeType>, public metric<gauge<TGaugeType &>>
{
public:
    inline explicit gauge(TGaugeType& ref) noexcept :
            gauges::referential_gauge<TGaugeType>(ref)
    { }
    gauge(const gauge& copy) = default;
    ~gauge() = default;

    gauge& operator=(const gauge& other) = default;
};

namespace internal
{

template<typename T, gauges::gauge_aggregation_type TAggregation>
struct default_metric_builder<cxxmetrics::gauge<std::function<T()>, TAggregation>>
{
    cxxmetrics::gauge<std::function<T()>, TAggregation> operator()() const
    {
        static std::function<T()> o = []() { return std::declval<T>(); };
        return cxxmetrics::gauge<std::function<T()>, TAggregation>(o);
    }
};

template<typename TGaugeType, gauges::gauge_aggregation_type TAggregation>
struct default_metric_builder<cxxmetrics::gauge<TGaugeType &, TAggregation>>
{
    cxxmetrics::gauge<TGaugeType&, TAggregation> operator()() const
    {
        static TGaugeType *r = nullptr;
        return cxxmetrics::gauge<TGaugeType&, TAggregation>(*r);
    }
};

template<typename TGaugeType, gauges::gauge_aggregation_type TAggregation>
struct default_metric_builder<cxxmetrics::gauge<TGaugeType *, TAggregation>>
{
    cxxmetrics::gauge<TGaugeType*, TAggregation> operator()() const
    {
        static TGaugeType *r = nullptr;
        return cxxmetrics::gauge<TGaugeType*, TAggregation>(r);
    }
};

}

}

#endif //CXXMETRICS_GAUGE_HPP
