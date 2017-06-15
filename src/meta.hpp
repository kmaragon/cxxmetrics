#ifndef CXXMETRICS_META_HPP
#define CXXMETRICS_META_HPP

namespace cxxmetrics
{

namespace templates
{

using duration_type = unsigned long long;

template <duration_type ... TElems>
struct duration_collection;

template<typename ...T>
struct static_concat;

template<duration_type ... TLeft, duration_type ... TRight, typename ... TTail>
struct static_concat<duration_collection<TLeft...>, duration_collection<TRight...>, TTail...>
{
    using type = typename static_concat<duration_collection<TLeft..., TRight...>, TTail...>::type;
};

template<duration_type ... TColl>
struct static_concat<duration_collection<TColl...>>
{
    using type = duration_collection<TColl...>;
};

template<bool, typename TTrue, typename TFalse>
struct static_if_else;

template<typename TTrue, typename TFalse>
struct static_if_else<true, TTrue, TFalse>
{
    using type = TTrue;
};

template<typename TTrue, typename TFalse>
struct static_if_else<false, TTrue, TFalse>
{
    using type = TFalse;
};

template<duration_type TLeft, duration_type TRight>
struct is_less_eq
{
    static constexpr bool value = (TLeft <= TRight);
};

template<duration_type TLeft, duration_type TRight>
struct are_equal
{
    static constexpr bool value = (TLeft == TRight);
};

template<typename TData>
struct static_partition;

template<duration_type TPivot, duration_type THead, duration_type ...TData>
struct static_partition<duration_collection<TPivot, THead, TData...>>
{
private:
    using left_ = typename static_partition<duration_collection<TPivot, TData...>>::left;
    using right_ = typename static_partition<duration_collection<TPivot, TData...>>::right;
public:
    using left = typename static_if_else<is_less_eq<THead, TPivot>::value, typename static_concat<duration_collection<THead>, left_>::type, left_>::type;
    using right = typename static_if_else<!is_less_eq<THead, TPivot>::value, typename static_concat<right_, duration_collection<THead>>::type, right_>::type;
};

template<duration_type TPivot>
struct static_partition<duration_collection<TPivot>>
{
    using left = duration_collection<TPivot>;
    using right = duration_collection<>;
};

template<typename TData>
struct static_sorter
{
private:
    using partitioned_ = static_partition<TData>;
    using left_ = typename partitioned_::left;
    using right_ = typename partitioned_::right;

    using sorted_left = static_sorter<left_>;
    using sorted_right = static_sorter<right_>;
public:
    using type = typename static_concat<typename sorted_left::type, typename sorted_right::type>::type;
};

template<duration_type TLeft, duration_type TRight>
struct static_sorter<duration_collection<TLeft, TRight>>
{
    using type = typename static_if_else<is_less_eq<TLeft, TRight>::value, duration_collection<TLeft, TRight>, duration_collection<TRight, TLeft>>::type;
};

template<duration_type TSingle>
struct static_sorter<duration_collection<TSingle>>
{
    using type = duration_collection<TSingle>;
};

template<>
struct static_sorter<duration_collection<>>
{
    using type = duration_collection<>;
};

template<typename TData>
struct static_uniq;

template<duration_type TFirst, duration_type TSecond, duration_type ...TTail>
struct static_uniq<duration_collection<TFirst, TSecond, TTail...>>
{
private:
    using tail_ = typename static_uniq<duration_collection<TTail...>>::type;
public:
    using type = typename static_if_else<are_equal<TFirst, TSecond>::value, typename static_concat<duration_collection<TFirst>, tail_>::type, typename static_concat<duration_collection<TFirst, TSecond>, tail_>::type>::type;
};

template<duration_type TFirst>
struct static_uniq<duration_collection<TFirst>>
{
    using type = duration_collection<TFirst>;
};

template<>
struct static_uniq<duration_collection<>>
{
    using type = duration_collection<>;
};

} // templates

} // cxxmetrics

#endif //CXXMETRICS_META_HPP
