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
  enum class Kind { kRead, kErase } kind_;
  float amount_;
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
        write_amount_(1.),
        read_amount_(1.),
        erase_amount_(1.),
        feedback_({.kind_ = Feedback::Kind::kRead, .amount_ = 0.}) {}

  const size_t& position() const { return position_; }
  const float& write_amount() const { return write_amount_; }
  const float& read_amount() const { return read_amount_; }
  const float& erase_amount() const { return erase_amount_; }
  const Feedback& feedback() const { return feedback_; }
};

enum class TargetParameter {
  Position,
  WriteAmount,
  ReadAmount,
  EraseAmount,
  Feedback,
};

class Target {
  TargetParameter parameter_;
  size_t head_idx_;

 public:
  Target(const Target&) = default;
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
};

class Config {
  std::array<Head, NUM_HEADS> heads_;
  std::array<LFO, NUM_LFOS> lfos_;
  float dry_;
  float wet_;

 public:
  const std::array<Head, NUM_HEADS>& heads() const { return heads_; }
  const std::array<LFO, NUM_LFOS>& lfos() { return lfos_; }
  const float& dry() const { return dry_; }
  const float& wet() const { return wet_; }
};

}  // namespace fridge::config
// NOLINTEND(clang-diagnostic-unused-*)

#endif  // CONFIG_H_
