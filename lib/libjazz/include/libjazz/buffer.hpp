#ifndef BUFFER_H_
#define BUFFER_H_

#include <array>
#include <cstddef>
#include <cstdlib>

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
  Loop() : zero_(0), buffer_() {};

  Sample& operator[](std::ptrdiff_t i) { return buffer_[real_index_(i)]; }
  const Sample& operator[](std::ptrdiff_t i) const {
    return buffer_[real_index_(i)];
  }

  constexpr size_t size() const { return Size; }

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

  // TODO simple forward iterator or smth
};

}  // namespace jazz::buffer

#endif  // BUFFER_H_
