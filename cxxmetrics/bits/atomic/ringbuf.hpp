#pragma once

#include <atomic>

namespace cxxmetrics::atomic

{
/**
 * \brief A simple fixed-size ring buffer implementation
 *
 * This is not a proper queue with expected queue semantics. It only guarantees
 * consistency of "length". So when iterating, there's no guarantee of order,
 * because the writer thread can surpass the reader thread and the reader will
 * just keep reading the new items. It's meant to be used in reservoirs
 *
 * \tparam T The type of element in the ring buffer.
 * \tparam Size The size of the ring buffer
 */
template <typename T, size_t Size>
class ringbuf {
  std::array<std::atomic<T>, Size> data_;

  // where the end of the list is.
  std::atomic<std::size_t> tail_;

  // how many elements in the ring buffer that are available
  std::atomic<std::size_t> size_;

public:
  class iterator {
    long offset_;
    std::size_t remaining_;
    T current_;
    const ringbuf *buf_;

  public:
    using value_type = T;
    using iterator_category = std::input_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using reference = T &;
    using pointer = T *;

    iterator() noexcept;

    iterator(const ringbuf *rb, int64_t offset = 0) noexcept;

    iterator(const iterator &it) noexcept = default;

    iterator &operator=(const iterator &other) noexcept = default;

    bool operator==(const iterator &other) const noexcept;

    bool operator!=(const iterator &other) const noexcept;

    iterator &operator++() noexcept;

    const T *operator->() const noexcept;

    inline T operator*() const noexcept { return current_; }
  };

  /**
   * \brief The default constructor
   */
  ringbuf() noexcept;

  /**
   * \brief Construct a ringbuf using an input iterator
   *
   * \tparam Input The type of iterator
   * \param start The start of the iterator
   * \param end The end of the iterator
   */
  template <typename Input>
  ringbuf(Input start, const Input &end) noexcept;

  /**
   * \brief Copy constructor
   */
  ringbuf(const ringbuf &other) noexcept;

  ~ringbuf() = default;

  /**
   * \brief Copy assignment operator
   */
  ringbuf &operator=(const ringbuf &other) noexcept;

  /**
   * \brief Get an iterator to the ring buffer
   */
  iterator begin() const noexcept;

  /**
   * \brief Get an iterator to the end of the ring buffer
   */
  iterator end() const noexcept;

  /**
   * \brief
   * \param elem
   */
  void push(const T &elem) noexcept;

  /**
   * \brief Get the size of the ring buffer
   *
   * \return the size of the ring buffer
   */
  size_t size() const noexcept;

private:
  friend class iterator;
};

template <typename T, size_t Size>
ringbuf<T, Size>::iterator::iterator() noexcept
    : offset_(-1), remaining_(0), current_{}, buf_(nullptr){};

template <typename T, size_t Size>
ringbuf<T, Size>::iterator::iterator(const ringbuf *rb, int64_t offset) noexcept
    : offset_(offset), remaining_(0), current_{}, buf_(rb) {
  remaining_ = buf_->size_.load(std::memory_order_relaxed);
  if (remaining_ && buf_)
    current_ = buf_->data_[offset_++].load(std::memory_order_relaxed);
}

template <typename T, size_t Size>
bool ringbuf<T, Size>::iterator::operator==(
    const iterator &other) const noexcept {
  if (remaining_ == 0)
    return other.remaining_ == 0;

  if (buf_ != other.buf_)
    return false;

  return other.offset_ == offset_;
}

template <typename T, size_t Size>
bool ringbuf<T, Size>::iterator::operator!=(
    const iterator &other) const noexcept {
  return !operator==(other);
}

template <typename T, size_t Size>
typename ringbuf<T, Size>::iterator &
ringbuf<T, Size>::iterator::operator++() noexcept {
  if (!remaining_)
    return *this;

  current_ = buf_->data_[(offset_++) % Size].load(std::memory_order_relaxed);
  remaining_--;

  return *this;
}

template <typename T, size_t Size>
const T *ringbuf<T, Size>::iterator::operator->() const noexcept {
  return &current_;
}

template <typename T, size_t Size>
ringbuf<T, Size>::ringbuf() noexcept : tail_(0), size_(0) {
  static_assert(Size > 1, "The ringbuffer must have a size of at least 2");
}

template <typename T, size_t Size>
template <typename Input>
ringbuf<T, Size>::ringbuf(Input start, const Input &end) noexcept
    : tail_(0), size_(0) {
  std::size_t i = 0;
  for (; start != end && i < Size; ++i, ++start)
    std::atomic_init(&data_[i], *start);

  std::atomic_init(&tail_, i);
}

template <typename T, size_t Size>
ringbuf<T, Size>::ringbuf(const ringbuf &other) noexcept
    : ringbuf(other.begin(), other.end()) {}

template <typename T, size_t Size>
ringbuf<T, Size> &ringbuf<T, Size>::operator=(const ringbuf &other) noexcept {
  auto it = other.begin();
  std::size_t i = 0;
  for (; it != other.end() && i < Size; ++i, ++it)
    data_[i].store(*it, std::memory_order_relaxed);

  size_.store(i, std::memory_order_relaxed);
  tail_.store(i, std::memory_order_relaxed);
  ;
}

template <typename T, size_t Size>
typename ringbuf<T, Size>::iterator ringbuf<T, Size>::begin() const noexcept {
  auto size = size_.load();
  if (size < Size)
    return iterator(this, 0);

  return iterator(this, tail_.load() % Size);
}

template <typename T, size_t Size>
typename ringbuf<T, Size>::iterator ringbuf<T, Size>::end() const noexcept {
  return iterator();
}

template <typename T, size_t Size>
void ringbuf<T, Size>::push(const T &elem) noexcept {
  auto writeloc = tail_.fetch_add(1, std::memory_order_relaxed);
  auto index = writeloc % Size;
  data_[index].store(elem, std::memory_order_relaxed);

  auto csize = size_.load(std::memory_order_relaxed);
  while (true) {
    if (index < csize)
      break;

    csize = index;
    if (size_.compare_exchange_weak(csize, index + 1, std::memory_order_relaxed,
                                    std::memory_order_relaxed))
      return;
  }
}

template <typename T, size_t Size>
size_t ringbuf<T, Size>::size() const noexcept {
  return size_.load(std::memory_order_relaxed);
};

} // namespace cxxmetrics::atomic
