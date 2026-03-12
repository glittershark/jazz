#ifndef FRIDGE_H_
#define FRIDGE_H_

#include <array>
#include <cstddef>
#include <optional>

namespace fridge {

constexpr const size_t NUM_HEADS = 8;
constexpr const size_t NUM_LFOS = 8;
constexpr const size_t MAX_TARGET_PARAMS = 64; // ??

namespace control {
// TODO: knobs and buttons and things
} // namespace control

namespace config {

struct Feedback {
  enum class Kind { kRead, kErase } kind_;
  float amount_;
};

class Head {
  // Position relative to the global clock
  size_t position_;
  float write_amount_;
  float read_amount_;
  float erase_amount_;
  Feedback feedback_;

public:
  Head()
      : position_(0), write_amount_(1.), read_amount_(1.), erase_amount_(1.),
        feedback_({.kind = Feedback::Kind::kRead, .amount = 0.}) {}

  size_t &position() { return position_; }
  float &write_amount() { return write_amount_; }
  float &read_amount() { return read_amount_; }
  float &erase_amount() { return erase_amount_; }
  Feedback &feedback() { return feedback_; }
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
  Target(const Target &) = default;
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
};

} // namespace config

namespace state {} // namespace state

} // namespace fridge

#endif // FRIDGE_H_
