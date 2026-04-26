#ifndef BUFFER_H_
#define BUFFER_H_

#include <array>
#include <cstddef>
#include <cstdlib>
#include <iterator>

namespace jazz::buffer {

/**
 * Loop buffer / circular array / pseudo-deque.
 *
 * The conceptual model is:
 * - can be indexed with both positive and negative numbers and will wrap around
 *   sanely (e.g., buf[-1] would refer to the end of the array)
 * - can call Push() / PushBack() on it to push a value onto the start/end,
 *   which pushes an pops an item off the other end of the array and returns it
 *   (since it's a fixed size). after this operation, buf[0] / buf[-1] will
 *   contain the item we just wrote.
 */
template <typename Sample, const std::size_t Size>
class Loop {
#ifdef UNIT_TEST
 public:
#endif

  std::array<Sample, Size> buffer_;
  std::ptrdiff_t zero_;

  std::size_t real_index_(std::ptrdiff_t i) const {
    auto rotated = zero_ + i;

    /*
     * NOTE: modulo in c++ is fucked up.
     */
    return rotated >= 0 ? rotated % Size : Size - 1 - ((-rotated - 1) % Size);
  }

 public:
  class Iterator {
    friend class Loop;
    Loop* loop_;
    std::ptrdiff_t i_;

    Iterator(Loop* loop, std::ptrdiff_t i) : loop_(loop), i_(i) {}

   public:
    // required by std::iterator_traits to know how this thing works
    using value_type = Sample;
    using difference_type = std::ptrdiff_t;
    using pointer = Sample*;
    using reference = Sample&;
    using iterator_category = std::random_access_iterator_tag;

    Sample& operator*() { return (*loop_)[i_]; }
    const Sample& operator*() const { return (*loop_)[i_]; }

    // TODO: consider which of these operators should be modulo-aware

    Iterator& operator++() {
      ++i_;
      return *this;
    }

    Iterator operator++(int) { return Iterator(loop_, i_++); }

    Iterator& operator--() {
      --i_;
      return *this;
    }

    Iterator operator--(int) { return Iterator(loop_, i_--); }

    bool operator==(const Iterator& rhs) const {
      return loop_ == rhs.loop_ && i_ == rhs.i_;
    }

    bool operator!=(const Iterator& rhs) const { return !operator==(rhs); }

    Iterator operator+(const std::ptrdiff_t rhs) const {
      return Iterator(loop_, i_ + rhs);
    }

    Iterator operator-(const std::ptrdiff_t rhs) const {
      return *this + (-rhs);
    }

    std::ptrdiff_t operator-(const Iterator& rhs) const { return i_ - rhs.i_; }

    bool operator<(const Iterator& rhs) const { return i_ < rhs.i_; }
  };

  Loop() : zero_(0), buffer_() {};

  Sample& operator[](std::ptrdiff_t i) { return buffer_[real_index_(i)]; }
  const Sample& operator[](std::ptrdiff_t i) const {
    return buffer_[real_index_(i)];
  }

  constexpr std::size_t size() const { return Size; }

  Sample Push(Sample s) {
    zero_ = real_index_(-1);
    Sample old = buffer_[zero_];
    buffer_[zero_] = s;
    return old;
  }

  Sample PushBack(Sample s) {
    Sample old = buffer_[zero_];
    buffer_[zero_] = s;
    zero_ = real_index_(1);
    return old;
  }

  Iterator begin() { return Iterator(this, 0); }
  Iterator end() { return Iterator(this, Size); }
};

}  // namespace jazz::buffer

#endif  // BUFFER_H_
