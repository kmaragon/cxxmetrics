#pragma once
#include <functional>
#include <type_traits>

namespace cxxmetrics::detail::concurrent {

template<
    typename Key,
    typename Hash = std::hash<Key>,
    typename KeyEqual = std::equal_to<Key>>
class set {
public:
  template<typename T>
  bool contains(T &&key) const
      noexcept(std::is_nothrow_invocable_v<KeyEqual, Key, T>);
};

} // namespace cxxmetrics::detail::concurrent