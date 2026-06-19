#pragma once

#include "libjazz/buffer.hpp"

namespace touch {

enum class Feature {
  None,
  Peak,
  Trough,
  ZeroCrossing,
};

class Processor {
 public:
  static constexpr size_t SAMPLE_RATE = 48000;
  static constexpr size_t BUFFER_SIZE = SAMPLE_RATE / 1000;  // 1 ms of audio

  float Process(float sample);

 private:
  float ddt;
  float ddt2;

  jazz::buffer::CircularArray<float, BUFFER_SIZE> buffer_;
};

}  // namespace touch
