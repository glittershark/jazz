#pragma once

#include <array>
#include <cstddef>

#include "libjazz/stereo_sample.hpp"

namespace la_sort {

inline constexpr size_t kMaxLength = 1024;
inline constexpr float kMaxSharpness = 32.0f;
inline constexpr float kMinLfoRate = 0.05f;
inline constexpr float kMaxLfoRate = 20.0f;

struct Config {
  float weight_center = 0.5f;
  float weight_sharpness = 2.0f;
  float dry = 0.0f;
  float wet = 1.0f;
  size_t length = 64;
  float volume_compensation = 1.0f;
  float lfo_rate = 1.0f;
  float lfo_depth = 0.0f;
};

// One free-running phase shared by the two audio channels.
class CenterLfo {
 public:
  explicit CenterLfo(float sample_rate = 48000.0f);
  float Process(const Config& config);

 private:
  double sample_period_;
  double phase_ = 0.0;
};

// Prepare at control rate, then share one immutable table across both channels.
class Parameters {
 public:
  Parameters();

  // Invalid settings leave the previous configuration intact. Do not mutate
  // the active parameters while an audio callback is reading them.
  bool SetConfig(const Config& config);

 private:
  friend class Effect;
  // Exponential weights decay geometrically away from the selected rank.
  // Store two starting weights and one decay step instead of an N-entry row.
  struct RankWeights {
    float left;
    float right;
    float decay_step;
    float makeup_gain;
    size_t pivot;
  };

  Config config_;
  std::array<RankWeights, kMaxLength> weights_{};
  size_t prepared_length_ = 0;
};

class Effect {
 public:
  explicit Effect(float sample_rate = 48000.0f) : center_lfo_(sample_rate) {}

  // Shrinking retains the newest samples; growing fills with new input.
  jazz::audio::StereoSample ProcessSample(const Parameters& parameters,
                                          jazz::audio::StereoSample sample);

 private:
  struct Channel {
    std::array<float, kMaxLength> history{};
    std::array<float, kMaxLength> sorted{};
  };

  void ShrinkWindow(size_t length);
  void InsertSample(Channel& channel, float sample, size_t length);
  jazz::audio::StereoSample ReadWindow(const Parameters& parameters,
                                       float center);

  Channel left_;
  Channel right_;
  size_t count_ = 0;
  size_t write_index_ = 0;
  CenterLfo center_lfo_;
};

// ---------------------------------------------------------------------------
// Validation
// ---------------------------------------------------------------------------

bool IsValid(const Config& config);

}  // namespace la_sort
