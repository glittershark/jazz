#ifndef JAZZ_FILTER_H_
#define JAZZ_FILTER_H_

#include <array>
#include <cstddef>
#include <iterator>

#include "buffer.hpp"

namespace jazz::filter {

template <size_t Order, typename Coeff = float, typename Sample = Coeff>
class InfiniteImpulse {
 public:
  /**
   * Construct an IIR filter of the form
   *        Σ num * input_history
   * ----------------------------------
   *    Σ (1 + denom) * output_history
   */
  InfiniteImpulse(std::array<Coeff, Order + 1> num,
                  std::array<Coeff, Order> denom)
      : num_(num), denom_(denom) {}

  /**
   * Filter a single Sample, returning the result of the filter and updating our
   * internal state accordingly.
   */
  Sample Filter(const Sample input) {
    // direct form II transpose IIR filter implementation, pretty efficient and
    // shouldn't have internal overflow issues

    Sample output = history_.PushBack(Sample{0}) + num_[0] * input;

    for (size_t i = 0; i < Order; ++i) {
      history_[i] += num_[i + 1] * input - denom_[i] * output;
    }

    return output;
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
  std::array<Coeff, Order + 1> num_;
  std::array<Coeff, Order> denom_;

  buffer::CircularArray<Sample, Order> history_;
};

}  // namespace jazz::filter

#endif  // JAZZ_FILTER_H_
