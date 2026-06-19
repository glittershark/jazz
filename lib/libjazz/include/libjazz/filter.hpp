#ifndef JAZZ_FILTER_H_
#define JAZZ_FILTER_H_

#include <array>
#include <cstddef>
#include <iterator>

#include "buffer.hpp"

namespace jazz::filter {

template <size_t Numerator, size_t Denominator, typename Coeff = float,
          typename Sample = Coeff>
class InfiniteImpulse {
 public:
  /**
   * Construct an IIR filter of the form
   *        Σ num * input_history
   * ----------------------------------
   *    Σ (1 + denom) * output_history
   */
  InfiniteImpulse(std::array<Coeff, Numerator> num,
                  std::array<Coeff, Denominator> denom)
      : num_(num), denom_(denom) {}

  /**
   * Filter a single Sample, returning the result of the filter and updating our
   * internal state accordingly.
   */
  Sample Filter(const Sample s) {
    // direct form I IIR filter implementation, free from overflow but not
    // necessarily the most efficient
    Sample accum = 0;

    input_history_.Push(s);

    for (size_t i = 0; i < num_.size(); ++i) {
      accum += num_[i] * input_history_[i];
    }

    for (size_t i = 0; i < denom_.size(); ++i) {
      accum -= denom_[i] * output_history_[i];
    }

    output_history_.Push(accum);
    return accum;
  }

  /**
   * Filter a range of samples through this thing.
   */
  template <std::forward_iterator It>
    requires std::same_as<std::iter_value_t<It>, Sample>
  void Filter(It begin, It end) {
    while (begin != end) {
      *begin = Filter(*begin);
      ++begin;
    }
  }

 private:
  std::array<Coeff, Numerator> num_;
  std::array<Coeff, Denominator> denom_;

  buffer::CircularArray<Sample, Numerator> input_history_;
  buffer::CircularArray<Sample, Denominator> output_history_;
};

}  // namespace jazz::filter

#endif  // JAZZ_FILTER_H_
