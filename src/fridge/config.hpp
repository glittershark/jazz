#ifndef CONFIG_H_
#define CONFIG_H_

#include <array>
#include <cstddef>
#include <optional>

#include "constants.hpp"

// TODO(aspen): Remove once this is all actually used
// NOLINTBEGIN(clang-diagnostic-unused-*)
namespace fridge::config {

struct Feedback {
  enum class Kind { kRead, kErase } kind_ = Kind::kRead;
  float amount_ = 0.0f;

  Kind& kind() { return kind_; }
  const Kind& kind() const { return kind_; }
  float& amount() { return amount_; }
  const float& amount() const { return amount_; }
};

class Head {
  // Current position in the buffer
  size_t position_;
  float write_amount_;
  float read_amount_;
  float erase_amount_;
  Feedback feedback_;

 public:
  Head()
      : position_(0),
        write_amount_(1.0f),
        read_amount_(1.0f),
        erase_amount_(1.0f),
        feedback_({.kind_ = Feedback::Kind::kRead, .amount_ = 0.0f}) {}

  size_t& position() { return position_; }
  const size_t& position() const { return position_; }
  float& write_amount() { return write_amount_; }
  const float& write_amount() const { return write_amount_; }
  float& read_amount() { return read_amount_; }
  const float& read_amount() const { return read_amount_; }
  float& erase_amount() { return erase_amount_; }
  const float& erase_amount() const { return erase_amount_; }
  Feedback& feedback() { return feedback_; }
  const Feedback& feedback() const { return feedback_; }
};

enum class TargetObject { kHead, kLFO, kMixer };

enum class TargetParameter {
  kPosition,
  kWriteAmount,
  kReadAmount,
  kEraseAmount,
  kFeedbackAmount,
  kRange,
  kMaxGrainSize,
  kMinGrainSize,
  kReverseChance,
  kTeleportChance,
  kPitchShiftChance,
  kLowOctaveChance,
  kHighOctaveChance,
  kDry,
  kWet,
};

class Target {
  TargetObject object_;
  TargetParameter parameter_;
  size_t object_idx_;

 public:
  Target()
      : object_(TargetObject::kHead),
        parameter_(TargetParameter::kPosition),
        object_idx_(0) {}
  Target(TargetObject object, TargetParameter parameter, size_t object_idx = 0)
      : object_(object), parameter_(parameter), object_idx_(object_idx) {}
  Target(const Target&) = default;

  TargetObject& object() { return object_; }
  const TargetObject& object() const { return object_; }
  TargetParameter& parameter() { return parameter_; }
  const TargetParameter& parameter() const { return parameter_; }
  size_t& object_idx() { return object_idx_; }
  const size_t& object_idx() const { return object_idx_; }
};

class LFO {
  // The duration over which the LFO oscillates
  size_t range_;
  size_t max_grain_size_;
  size_t min_grain_size_;
  // Value between 0 and 1
  float reverse_chance_;
  // Value between 0 and 1
  float teleport_chance_;
  // Value between 0 and 1
  float pitch_shift_chance_;
  // Value between 0 and 1
  float low_octave_chance_;
  // Value between 0 and 1
  float high_octave_chance_;

  std::array<std::optional<Target>, MAX_TARGET_PARAMS> targets_;

 public:
  LFO(size_t range = 0, size_t max_grain_size = 1, size_t min_grain_size = 1,
      float reverse_chance = 0.0f, float teleport_chance = 0.0f,
      float pitch_shift_chance = 0.0f, float low_octave_chance = 0.0f,
      float high_octave_chance = 0.0f)
      : range_(range),
        max_grain_size_(max_grain_size),
        min_grain_size_(min_grain_size),
        reverse_chance_(reverse_chance),
        teleport_chance_(teleport_chance),
        pitch_shift_chance_(pitch_shift_chance),
        low_octave_chance_(low_octave_chance),
        high_octave_chance_(high_octave_chance) {}

  size_t& range() { return range_; }
  const size_t& range() const { return range_; }
  size_t& max_grain_size() { return max_grain_size_; }
  const size_t& max_grain_size() const { return max_grain_size_; }
  size_t& min_grain_size() { return min_grain_size_; }
  const size_t& min_grain_size() const { return min_grain_size_; }
  float& reverse_chance() { return reverse_chance_; }
  const float& reverse_chance() const { return reverse_chance_; }
  float& teleport_chance() { return teleport_chance_; }
  const float& teleport_chance() const { return teleport_chance_; }
  float& pitch_shift_chance() { return pitch_shift_chance_; }
  const float& pitch_shift_chance() const { return pitch_shift_chance_; }
  float& low_octave_chance() { return low_octave_chance_; }
  const float& low_octave_chance() const { return low_octave_chance_; }
  float& high_octave_chance() { return high_octave_chance_; }
  const float& high_octave_chance() const { return high_octave_chance_; }
  std::array<std::optional<Target>, MAX_TARGET_PARAMS>& targets() {
    return targets_;
  }
  const std::array<std::optional<Target>, MAX_TARGET_PARAMS>& targets() const {
    return targets_;
  }
};

class Config {
  std::array<Head, NUM_HEADS> heads_;
  std::array<LFO, NUM_LFOS> lfos_;
  float dry_ = 1.0f;
  float wet_ = 1.0f;

 public:
  std::array<Head, NUM_HEADS>& heads() { return heads_; }
  const std::array<Head, NUM_HEADS>& heads() const { return heads_; }
  std::array<LFO, NUM_LFOS>& lfos() { return lfos_; }
  const std::array<LFO, NUM_LFOS>& lfos() const { return lfos_; }
  float& dry() { return dry_; }
  const float& dry() const { return dry_; }
  float& wet() { return wet_; }
  const float& wet() const { return wet_; }
};

}  // namespace fridge::config
// NOLINTEND(clang-diagnostic-unused-*)

#endif  // CONFIG_H_
