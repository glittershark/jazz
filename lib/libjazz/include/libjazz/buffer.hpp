#ifndef BUFFER_H_
#define BUFFER_H_

#include <array>
#include <cstddef>
#include <cstdlib>
#include <iterator>
#include <ostream>

namespace jazz::buffer {

/*
 * Random-access iterator class for indexable stuff. Getting this right is
 * surprisingly tricky, so we'll just do this once. Right here. Right now.
 */
template <typename A, typename T>
class Iterator {
  /*
   * NOTE: we can't static-assert that this is a std::random_access_iterator
   * because it will fail if this class is instantiated by code that doesn't use
   * all of the iterator features (e.g., we never call operator--() for some
   * reason). the concept check in the static_assert() doesn't force this for
   * whatever reason due to template bullshit, so we have to rely on the tests.
   *
   * at least, that's what i /think/ is going on here...
   */

  friend A;

  A* array_;
  std::ptrdiff_t i_;

  // NOTE: only constructible by A
  Iterator(A* array, std::ptrdiff_t i) : array_(array), i_(i) {}

 public:
  // required by std::iterator_traits to know how this thing works
  using value_type = T;
  using difference_type = std::ptrdiff_t;
  using pointer = value_type*;
  using reference = value_type&;
  using iterator_category = std::random_access_iterator_tag;

  value_type& operator*() const { return (*array_)[i_]; }
  value_type& operator[](std::ptrdiff_t i) const { return (*array_)[i]; }

  // FIXME: for some reason std::random_access_iterator requires this thing to
  // be /default-constructible/, which is NEVER a good idea for iterators...!?
  Iterator() : array_(nullptr), i_(0) {}

  // TODO: consider which of these operators should be modulo-aware

  Iterator& operator++() {
    ++i_;
    return *this;
  }

  Iterator operator++(int) { return Iterator(array_, i_++); }

  Iterator& operator--() {
    --i_;
    return *this;
  }

  Iterator operator--(int) { return Iterator(array_, i_--); }

  bool operator==(const Iterator& rhs) const {
    return array_ == rhs.array_ && i_ == rhs.i_;
  }

  bool operator!=(const Iterator& rhs) const { return !operator==(rhs); }

  Iterator operator+(const difference_type rhs) const {
    return Iterator(array_, i_ + rhs);
  }

  friend Iterator operator+(const difference_type lhs, const Iterator& rhs) {
    return rhs + lhs;
  }

  Iterator operator-(const difference_type rhs) const { return *this + (-rhs); }

  difference_type operator-(const Iterator& rhs) const { return i_ - rhs.i_; }

  Iterator& operator+=(const difference_type rhs) {
    i_ += rhs;
    return *this;
  }

  Iterator& operator-=(const difference_type rhs) {
    i_ -= rhs;
    return *this;
  }

  bool operator<(const Iterator& rhs) const { return i_ < rhs.i_; }
  bool operator>(const Iterator& rhs) const { return i_ > rhs.i_; }
  bool operator<=(const Iterator& rhs) const { return i_ <= rhs.i_; }
  bool operator>=(const Iterator& rhs) const { return i_ >= rhs.i_; }
};

/**
 * Circular array / loop buffer / pseudo-deque.
 *
 * The conceptual model is:
 * - can be indexed with both positive and negative numbers and will wrap around
 *   sanely (e.g., buf[-1] would refer to the end of the array)
 * - can call Push() / PushBack() on it to push a value onto the start/end,
 *   which pushes an pops an item off the other end of the array and returns it
 *   (since it's a fixed size). after this operation, buf[0] / buf[-1] will
 *   contain the item we just wrote.
 */
template <typename T, const std::size_t Size>
class CircularArray {
#ifdef UNIT_TEST
 public:
#endif
  std::array<T, Size> buffer_;
  std::ptrdiff_t zero_;

  /*
   * Compute the real index into buffer_, given a virtual index into the
   * conceptual circular array. Used for both indexing into buffer_ and moving
   * zero_ around conveniently. Just an implementation detail, but used in
   * several places and tricky to get right.
   *
   * This function does two things: it
   * 1. shifts the input index according to the current "front" of the array
   *    (zero_), which changes when pushes/pops occur, and
   * 2. performs an /unsigned/ modulus on the /signed/ shifted result so that we
   *    wrap around the end of the array.
   */
  std::size_t real_index_(std::ptrdiff_t i) const {
    auto rotated = zero_ + i;

    /*
     * NOTE: modulo in c++ is fucked up. this performs an /unsigned/ modulus
     * (e.g., 1 % 4 == 1, -1 % 4 == 3).
     *
     * TODO: consider restricting to power-of-two size so this whole horrible
     * business is just a bitmask.
     */
    return rotated >= 0 ? rotated % Size : Size - 1 - ((-rotated - 1) % Size);
  }

 public:
  CircularArray() : zero_(0), buffer_() {};

  /*
   * Standard C++ random-access container functions.
   */
  using iterator = Iterator<CircularArray, T>;
  using const_iterator = Iterator<const CircularArray, const T>;

  iterator begin() { return iterator(this, 0); }
  const_iterator begin() const { return const_iterator(this, 0); }

  iterator end() { return iterator(this, Size); }
  const_iterator end() const { return const_iterator(this, Size); }

  constexpr std::size_t size() const { return Size; }

  T& operator[](std::ptrdiff_t i) { return buffer_[real_index_(i)]; }
  const T& operator[](std::ptrdiff_t i) const {
    return buffer_[real_index_(i)];
  }

  /*
   * Pushes an element onto the front (index 0) of the array, popping off the
   * element at the back (index -1) and returning it. All elements shift forward
   * one position.
   */
  T Push(T s) {
    zero_ = real_index_(-1);
    T old = buffer_[zero_];
    buffer_[zero_] = s;
    return old;
  }

  /*
   * Pushes an element onto the back (index -1) of the array, popping off the
   * element at the front (index 0) and returning it. All elements shift
   * backward one position.
   */
  T PushBack(T s) {
    T old = buffer_[zero_];
    buffer_[zero_] = s;
    zero_ = real_index_(1);
    return old;
  }
};

}  // namespace jazz::buffer

template <typename T, std::size_t Size>
std::ostream& operator<<(std::ostream& os,
                         const jazz::buffer::CircularArray<T, Size>& a) {
  os << "CircularArray{";

  for (auto i = a.begin(); i != a.end() - 1; ++i) {
    os << *i << ", ";
  }

  os << a[-1] << '}';
  return os;
}

#endif  // BUFFER_H_
