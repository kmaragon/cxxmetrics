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
struct is_less
{
    static constexpr bool value = (TLeft < TRight);
};

template<duration_type TLeft, duration_type TRight>
struct is_greater
{
    static constexpr bool value = (TLeft > TRight);
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
    using middle_ = typename static_partition<duration_collection<TPivot, TData...>>::middle;
    using right_ = typename static_partition<duration_collection<TPivot, TData...>>::right;
public:
    using left = typename static_if_else<is_less<THead, TPivot>::value, typename static_concat<duration_collection<THead>, left_>::type, left_>::type;
    using middle = typename static_if_else<are_equal<THead, TPivot>::value, typename static_concat<duration_collection<THead>, middle_>::type, middle_>::type;
    using right = typename static_if_else<is_greater<THead, TPivot>::value, typename static_concat<duration_collection<THead>, right_>::type, right_>::type;
};

template<duration_type TPivot>
struct static_partition<duration_collection<TPivot>>
{
    using left = duration_collection<>;
    using middle = duration_collection<TPivot>;
    using right = duration_collection<>;
};

template<typename TData>
struct static_sorter;

template<duration_type...TData>
struct static_sorter<duration_collection<TData...>>
{
private:
    using partitioned_ = static_partition<duration_collection<TData...>>;

    using left_ = typename static_sorter<typename partitioned_::left>::type;
    using middle_ = typename partitioned_::middle;
    using right_ = typename static_sorter<typename partitioned_::right>::type;
public:
    using type = typename static_concat<left_, middle_, right_>::type;
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
    using type = typename static_if_else<are_equal<TFirst, TSecond>::value, typename static_uniq<duration_collection<TFirst, TTail...>>::type, typename static_concat<duration_collection<TFirst>, typename static_uniq<duration_collection<TSecond, TTail...>>::type>::type>::type;
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

template<duration_type ... durations>
struct sort_unique
{
private:
    using list_ = duration_collection<durations...>;
    using sorted_ = typename static_sorter<list_>::type;
public:
    using type = typename static_uniq<sorted_>::type;
};

} // templates

} // cxxmetrics

#endif //CXXMETRICS_META_HPP
