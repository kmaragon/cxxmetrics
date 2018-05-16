#ifndef CXXMETRICS_ATOMIC_LIFO_HPP
#define CXXMETRICS_ATOMIC_LIFO_HPP

#include <initializer_list>
#include <memory>
#include <atomic>
#include <type_traits>

namespace cxxmetrics {
namespace internal {

// concepts would be swell
template<typename T>
class list_node_traits
{
    struct bad_type {};

    static T* as_pointer(T* value) { return value; }
    static T* as_pointer(T& value) { return &value; }
    template<typename C> static void as_pointer(C);

    template<typename C>
    static auto get_next_impl(C& obj) -> decltype(obj.next()) { return obj.next(); }
    template<typename C>
    static int get_next_impl(...);

    template<typename C>
    static auto set_next_impl(C& obj, C* value) -> decltype(obj.set_next(value))
    {
        obj.set_next(value);
    }
    template<typename C>
    static bad_type set_next_impl(C& obj, ...);

    template<typename C>
    static auto call_value_impl(C& obj) -> decltype(obj.value())
    {
        return obj.value();
    }
    template<typename C>
    static bad_type call_value_impl(...);
    using call_value_result = decltype(call_value_impl<T>(*reinterpret_cast<T*>(0)));

    template<typename C, typename VType>
    static constexpr bool has_get_for = std::is_convertible<call_value_result, VType&>::value;
    template<typename C, typename VType>
    static typename std::enable_if<has_get_for<C, VType>, VType>::type& get_value_impl(C& obj)
    {
        return static_cast<VType&>(call_value_impl(obj));
    }
    template<typename C, typename VType>
    static typename std::enable_if<!has_get_for<C, VType> && std::is_convertible<C&, VType&>::value, VType>::type& get_value_impl(C& obj)
    {
        return static_cast<VType&>(obj);
    }
    template<typename C, typename VType>
    static void get_value_impl(...);

    static constexpr bool has_valid_next = std::is_same<decltype(as_pointer(get_next_impl<T>(*reinterpret_cast<T*>(0)))), T*>::value;
    static constexpr bool has_valid_set_next = !std::is_same<decltype(set_next_impl(*reinterpret_cast<T*>(0), reinterpret_cast<T*>(0))), bad_type>::value;
    template<typename Value>
    static constexpr bool has_value_conversion_for = std::is_same<decltype(get_value_impl<T, Value>(*reinterpret_cast<T*>(0))), Value&>::value;
public:

    template<typename Value>
    static constexpr bool is_valid_node_of = has_valid_next && has_valid_set_next && has_value_conversion_for<Value> && std::is_copy_constructible<T>::value;

    static T* next_of(T& obj)
    {
        return as_pointer(get_next_impl<T>(obj));
    }

    static auto set_next_of(T& obj, T* next)
    {
        set_next_impl<T>(obj, next);
    }

    template<typename V>
    static auto& value_of(T& obj)
    {
        static_assert(has_value_conversion_for<V>, "No conversion path for the specified type");
        return get_value_impl<T, V>(obj);
    }
};

template<typename T, typename Alloc = std::allocator<T>, bool UseTAsNode = list_node_traits<T>::template is_valid_node_of<const T&>>
class atomic_lifo
{
	struct contained_node {
		T value_;
		contained_node* next_;

		template<typename... TInitArgs>
		contained_node(TInitArgs&&... args) :
                value_(std::forward<TInitArgs>(args)...),
                next_(nullptr)
        { }

		contained_node* next() const {
			return next_;
		}

		void set_next(contained_node* n)
		{
			next_ = n;
		}

		operator T&() {
			return value_;
		}

		operator const T&() const {
			return value_;
		}
	};

    using node = typename std::conditional<UseTAsNode, T, contained_node>::type;

public:
    using value_type = typename std::conditional<UseTAsNode, node, T>::type;
    using allocator_type = typename std::allocator_traits<Alloc>::template rebind_alloc<node>;

private:
    class deallocator
    {
        allocator_type allocator_;
    public:
        deallocator(const allocator_type& alloc = allocator_type()) :
                allocator_(alloc)
        { }

        void operator()(node* n)
        {
            std::allocator_traits<allocator_type>::destroy(allocator_, n);
            std::allocator_traits<allocator_type>::deallocate(allocator_, n, 1);
        }
    };

public:
    class pointer_type
    {
        std::unique_ptr<node, deallocator> ptr_;
    public:
        pointer_type(const allocator_type& allocator = allocator_type()) :
                ptr_(nullptr, { allocator })
        { }

        pointer_type(node* node, const allocator_type& alloc = allocator_type()) :
                ptr_(node, { alloc })
        { }

        pointer_type(pointer_type&& other) noexcept :
            ptr_(std::move(other.ptr_))
        { }

        pointer_type& operator=(const pointer_type&) = delete;
        pointer_type& operator=(pointer_type&& other)
        {
            ptr_ = std::move(other.ptr_);
            return *this;
        }

        const value_type& operator*() const
        {
            return list_node_traits<node>::template value_of<value_type>(*ptr_);
        }

        value_type& operator*()
        {
            return list_node_traits<node>::template value_of<value_type>(*ptr_);
        }

        value_type* operator->() const
        {
            return &list_node_traits<node>::template value_of<value_type>(*ptr_);
        }

        value_type* operator->()
        {
            return &list_node_traits<node>::template value_of<value_type>(*ptr_);
        }

        operator bool() const {
            return static_cast<bool>(ptr_);
        }
    };

private:
	allocator_type alloc_;
	std::atomic<node*> head_;

public:

	constexpr atomic_lifo() :
			head_(nullptr)
	{ }

	template<typename Begin, typename End = Begin>
	atomic_lifo(Begin begin, const End& end) :
			head_(nullptr)
	{
		for (; begin != end; ++begin)
			push(*begin);
	}

	atomic_lifo(const std::initializer_list<T>& list) :
			atomic_lifo(std::rbegin(list), std::rend(list))
	{ }

	atomic_lifo(const atomic_lifo&) = delete;

	virtual ~atomic_lifo()
    {
        deallocator a;
        auto oh = head_.exchange(nullptr);
        while (oh)
        {
            auto tmp = oh;
            oh = oh->next();

            a(tmp);
        }
    }

	void push(T value)
	{
        return emplace(std::move(value));
	}

	template<typename... Args>
	void emplace(Args&&... args)
	{
		auto n = std::allocator_traits<allocator_type>::allocate(alloc_, 1);
		std::allocator_traits<allocator_type>::construct(alloc_, n, std::forward<Args>(args)...);

        auto h = head_.load();
		while (true)
		{
			list_node_traits<node>::set_next_of(*n, h);
			if (head_.compare_exchange_weak(h, n))
			    break;
		}
	}

	pointer_type pop()
    {
        auto h = head_.load();

        while (true)
        {
            if (!h)
                break;

            auto n = list_node_traits<node>::next_of(*h);
            if (head_.compare_exchange_weak(h, n))
                break;
        }

        return pointer_type(h, alloc_);
    }
};

}
}

#endif //CXXMETRICS_ATOMIC_LIFO_HPP
