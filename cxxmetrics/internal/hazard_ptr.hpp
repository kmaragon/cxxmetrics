#ifndef CXXMETRICS_HAZARD_PTR_HPP
#define CXXMETRICS_HAZARD_PTR_HPP

#include "atomic_lifo.hpp"
#include "memory_resource_shim.hpp"

namespace cxxmetrics
{

namespace detail
{

class hazptr_domain {
public:
    constexpr explicit hazptr_domain(std::pmr::memory_resource* resource = std::pmr::get_default_resource()) :
            mr(resource)
    { }

    hazptr_domain(const hazptr_domain&) = delete;
    hazptr_domain(hazptr_domain&&) = delete;
    hazptr_domain& operator=(const hazptr_domain&) = delete;
    hazptr_domain& operator=(hazptr_domain&&) = delete;

    virtual ~hazptr_domain() {

    }
private:
    std::pmr::memory_resource* mr; // exposition only
};

inline hazptr_domain& default_hazptr_domain() {
    static hazptr_domain d;
    return d;
}

template <typename T, typename D = std::default_delete<T>>
class hazptr_obj_base {
public:
    void retire(D reclaim = {}, hazptr_domain& domain = default_hazptr_domain());
    void retire(hazptr_domain& domain);
};

class hazptr_holder {
public:
    explicit hazptr_holder(hazptr_domain& domain = default_hazptr_domain());
    explicit hazptr_holder(nullptr_t) noexcept;
    hazptr_holder(hazptr_holder&&) noexcept;

    hazptr_holder(const hazptr_holder&) = delete;
    hazptr_holder& operator=(const hazptr_holder&) = delete;

    ~hazptr_holder();

    hazptr_holder& operator=(hazptr_holder&&) noexcept;

    explicit operator bool() const noexcept;

    template <typename T>
    T* get_protected(const std::atomic<T*>& src) noexcept;

    template <typename T>
    bool try_protect(T*& ptr, const std::atomic<T*>& src) noexcept;

    template <typename T>
    void reset(const T* ptr) noexcept;
    void reset(nullptr_t = nullptr) noexcept;

    void swap(hazptr_holder&) noexcept;
};

}

}

#endif // CXXMETRICS_HAZARD_PTR_HPP
