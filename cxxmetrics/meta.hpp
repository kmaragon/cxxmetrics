#ifndef CXXMETRICS_META_HPP
#define CXXMETRICS_META_HPP

namespace cxxmetrics
{

namespace templates
{

using sortable_template_type = unsigned long long;

template <sortable_template_type ... TElems>
struct sortable_template_collection;

template<typename ...T>
struct static_concat;

template<sortable_template_type ... TLeft, sortable_template_type ... TRight, typename ... TTail>
struct static_concat<sortable_template_collection<TLeft...>, sortable_template_collection<TRight...>, TTail...>
{
    using type = typename static_concat<sortable_template_collection<TLeft..., TRight...>, TTail...>::type;
};

template<sortable_template_type ... TColl>
struct static_concat<sortable_template_collection<TColl...>>
{
    using type = sortable_template_collection<TColl...>;
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

template<sortable_template_type TLeft, sortable_template_type TRight>
struct is_less
{
    static constexpr bool value = (TLeft < TRight);
};

template<sortable_template_type TLeft, sortable_template_type TRight>
struct is_greater
{
    static constexpr bool value = (TLeft > TRight);
};

template<sortable_template_type TLeft, sortable_template_type TRight>
struct are_equal
{
    static constexpr bool value = (TLeft == TRight);
};

template<typename TData>
struct static_partition;

template<sortable_template_type TPivot, sortable_template_type THead, sortable_template_type ...TData>
struct static_partition<sortable_template_collection<TPivot, THead, TData...>>
{
private:
    using left_ = typename static_partition<sortable_template_collection<TPivot, TData...>>::left;
    using middle_ = typename static_partition<sortable_template_collection<TPivot, TData...>>::middle;
    using right_ = typename static_partition<sortable_template_collection<TPivot, TData...>>::right;
public:
    using left = typename static_if_else<is_less<THead, TPivot>::value, typename static_concat<sortable_template_collection<THead>, left_>::type, left_>::type;
    using middle = typename static_if_else<are_equal<THead, TPivot>::value, typename static_concat<sortable_template_collection<THead>, middle_>::type, middle_>::type;
    using right = typename static_if_else<is_greater<THead, TPivot>::value, typename static_concat<sortable_template_collection<THead>, right_>::type, right_>::type;
};

template<sortable_template_type TPivot>
struct static_partition<sortable_template_collection<TPivot>>
{
    using left = sortable_template_collection<>;
    using middle = sortable_template_collection<TPivot>;
    using right = sortable_template_collection<>;
};

template<typename TData>
struct static_sorter;

template<sortable_template_type...TData>
struct static_sorter<sortable_template_collection<TData...>>
{
private:
    using partitioned_ = static_partition<sortable_template_collection<TData...>>;

    using left_ = typename static_sorter<typename partitioned_::left>::type;
    using middle_ = typename partitioned_::middle;
    using right_ = typename static_sorter<typename partitioned_::right>::type;
public:
    using type = typename static_concat<left_, middle_, right_>::type;
};

template<>
struct static_sorter<sortable_template_collection<>>
{
    using type = sortable_template_collection<>;
};

template<typename TData>
struct static_uniq;

template<sortable_template_type TFirst, sortable_template_type TSecond, sortable_template_type ...TTail>
struct static_uniq<sortable_template_collection<TFirst, TSecond, TTail...>>
{
    using type = typename static_if_else<are_equal<TFirst, TSecond>::value, typename static_uniq<sortable_template_collection<TFirst, TTail...>>::type, typename static_concat<sortable_template_collection<TFirst>, typename static_uniq<sortable_template_collection<TSecond, TTail...>>::type>::type>::type;
};

template<sortable_template_type TFirst>
struct static_uniq<sortable_template_collection<TFirst>>
{
    using type = sortable_template_collection<TFirst>;
};

template<>
struct static_uniq<sortable_template_collection<>>
{
    using type = sortable_template_collection<>;
};

template<sortable_template_type ... durations>
struct sort_unique
{
private:
    using list_ = sortable_template_collection<durations...>;
    using sorted_ = typename static_sorter<list_>::type;
public:
    using type = typename static_uniq<sorted_>::type;
};

} // templates

} // cxxmetrics

#endif //CXXMETRICS_META_HPP
