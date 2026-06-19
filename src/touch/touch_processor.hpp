#pragma once

#include "libjazz/buffer.hpp"
#include "libjazz/filter.hpp"

namespace touch {

enum class Feature {
  None,
  ZeroCrossing,
  Peak,
  Trough,
  Inflection,
};

class Processor {
 public:
  static constexpr size_t SAMPLE_RATE = 48000;
  static constexpr size_t HISTORY_SIZE = 2;

  using HistoryBuffer = jazz::buffer::CircularArray<float, HISTORY_SIZE>;

  Processor();
  float Process(float sample);

 private:
  jazz::filter::InfiniteImpulse<4> history_filter_;
  jazz::filter::InfiniteImpulse<5> output_filter_;

  HistoryBuffer history_;
  HistoryBuffer velocity_;
  HistoryBuffer acceleration_;

  float zero_crossings_;
  float spikes_;

  Feature LatestFeature();
};

}  // namespace touch
