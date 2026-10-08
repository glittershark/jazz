#ifndef FRIDGE_LFO_H_
#define FRIDGE_LFO_H_

#include <cstdint>
#include <optional>

#include "config.hpp"
#include "libjazz/units.hpp"

namespace fridge::mod {

using jazz::units::Samples;

enum class Direction { kForwards = 1, kBackwards = -1 };

/** xorshift32: 4 bytes of state, a few cycles per draw. */
class Rng {
 public:
  explicit Rng(uint32_t seed = 1);

  uint32_t Next();
  /** Uniform float in [0, 1). */
  float NextFloat();
  /** Returns true with probability `chance` (clamped to [0, 1]). */
  bool Chance(float chance);
  /** Uniform integer in [lo, hi]. */
  uint32_t Between(uint32_t lo, uint32_t hi);

 private:
  uint32_t state_;
};

struct LFOTransition {
  float old_value = 0.0f;
  Direction old_direction = Direction::kForwards;
  float old_speed = 1.0f;
  float new_value = 0.0f;
  Direction new_direction = Direction::kForwards;
  float new_speed = 1.0f;
  bool reversed = false;
  bool teleported = false;
};

struct LFOTickResult {
  float value = 0.0f;
  std::optional<LFOTransition> transition = std::nullopt;
};

/** The scalar parameters an LFO engine actually consumes (everything in
 * config::LFO except the target list). */
struct LfoParams {
  size_t range = 0;
  size_t max_grain_size = 1;
  size_t min_grain_size = 1;
  float reverse_chance = 0.0f;
  float teleport_chance = 0.0f;
  float pitch_shift_chance = 0.0f;
  float low_octave_chance = 0.0f;
  float high_octave_chance = 0.0f;
};

/**
 * One granular LFO voice. Piecewise linear: between grain boundaries the
 * value advances by speed * direction each sample, wrapped to [0, range).
 */
class LFOEngine {
 public:
  LFOEngine() = default;
  explicit LFOEngine(const config::LFO& config, uint32_t seed = 1);

  /** Optionally preserve relative progress when the scalar range changes;
   * speed, direction, grain timing, and RNG state remain untouched. */
  void SetParams(const LfoParams& params, bool preserve_phase = false);
  void SetConfig(const config::LFO& config);
  void Reset(float initial_value = 0.0f,
             Direction direction = Direction::kForwards);
  float Tick(Samples<uint32_t> step);
  LFOTickResult TickWithEvents(Samples<uint32_t> step);

  const LfoParams& params() const { return params_; }
  /** The engine's parameters as a config::LFO (with an empty target list —
   * the engine never reads targets). */
  config::LFO config() const;
  float value() const { return value_; }
  float speed() const { return speed_; }
  float grain_time_remaining() const {
    return static_cast<float>(grain_remaining_);
  }
  size_t grain_size() const { return grain_size_; }
  Direction direction() const { return direction_; }

 private:
  LfoParams params_{};
  Rng rng_{};
  float value_ = 0.0f;
  float speed_ = 1.0f;
  uint32_t grain_remaining_ = 0;
  uint32_t grain_size_ = 0;
  Direction direction_ = Direction::kForwards;

  std::optional<LFOTransition> StartNewGrain(bool initial_grain);
  uint32_t SampleGrainSize();
  float SampleSpeed();
  float Wrap(float value) const;

  // ----- Validation
  static LfoParams SanitizeParams(LfoParams params);
};

}  // namespace fridge::mod

#endif
