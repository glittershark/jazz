#ifndef FRIDGE_MOD_H_
#define FRIDGE_MOD_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "config.hpp"
#include "constants.hpp"
#include "libjazz/units.hpp"

/**
 * The modulation system: granular LFOs, the routing of their values onto
 * config parameters, and the crossfades that smooth head-position jumps.
 *
 * The design splits work by how often it can change:
 *  - per config change (knob moves):  sanitize the root config and compile
 *    the active modulation routes — Modulator::SetConfig
 *  - per grain (every min..max_grain_size samples): all randomness — reverse /
 *    teleport / pitch rolls
 *  - per sample: each LFO value advances linearly by speed * direction —
 *    Modulator::TickSample is a handful of adds
 */
namespace fridge::mod {

using jazz::units::Samples;

enum class Direction { kForwards = 1, kBackwards = -1 };

/** xorshift32: 4 bytes of state, a few cycles per draw. */
class Rng {
  uint32_t state_;

 public:
  explicit Rng(uint32_t seed = 1);

  uint32_t Next();
  /** Uniform float in [0, 1). */
  float NextFloat();
  /** Returns true with probability `chance` (clamped to [0, 1]). */
  bool Chance(float chance);
  /** Uniform integer in [lo, hi]. */
  uint32_t Between(uint32_t lo, uint32_t hi);
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

 public:
  LFOEngine() = default;
  explicit LFOEngine(const config::LFO& config);
  LFOEngine(const config::LFO& config, uint32_t seed);

  void SetParams(const LfoParams& params);
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
};

/** What Sound consumes each sample: scaled head contributions plus the mix. */
struct Frame {
  std::array<config::Head, kNumHeads * 2> heads{};
  size_t head_count = 0;
  float dry = 1.0f;
  float wet = 1.0f;
};

/**
 * Owns the LFO engines, applies their deltas onto the root config, and
 * crossfades head positions through reversals and teleports.
 *
 * Audio path:    TickSample() once per sample, feed frame() to Sound.
 * Control path:  SetConfig() whenever the root config changes (keeps LFO
 *                phase and modulation), Reset() to start over.
 * Host/test:     Update(root, step) detects root changes by comparison and
 *                returns the modulated config materialized as a config::Config.
 */
class Modulator {
 public:
  /** Default seed is fixed: std::random_device hard-faults on the bare-metal
   * target (newlib has no entropy source), so hosts that want per-run
   * randomness must pass their own seed. */
  explicit Modulator(uint32_t seed = 1, size_t fade_time = kFadeTime);

  /** Replace the root config. LFO phase and current modulation offsets are
   * preserved; the new base takes effect on the next sample. */
  void SetConfig(const config::Config& root_config);

  /** Re-initialize from scratch: reseeds LFOs, clears fades, time = 0. */
  const config::Config& Reset(const config::Config& root_config);

  /** Advance the world by one sample. */
  const Frame& TickSample();

  /** Compatibility entry point for hosts that pass the root config every
   * call: rebases if `root_config` changed, ticks `step` samples, and
   * returns the modulated config. */
  const config::Config& Update(const config::Config& root_config,
                               Samples<uint32_t> step);

  /** The modulated config, materialized on demand. */
  const config::Config& virtual_config();

  struct Fade {
    config::Head old_head{};  // unscaled head params at the moment of the jump
    float old_position = 0.0f;  // keeps moving with the old motion during fade
    float old_velocity = 0.0f;  // signed samples per sample
    uint32_t remaining = 0;     // samples left; 0 → inactive
  };

  const Frame& frame() const { return frame_; }
  const std::array<Fade, kNumHeads>& fades() const { return fades_; }
  Samples<uint32_t> time() const { return time_; }

  // ----- Validation (shared with materialization)

  static float ClampFinite(float value, float fallback = 0.0f);
  static float ClampChance(float value);
  static size_t ClampSize(float value, size_t minimum);
  /** audio::Pan asserts on anything outside [-1, 1], so modulation has to be
   * clamped before it can be handed back as one. */
  static config::Pan ClampPan(float value);
  static config::Config SanitizeConfig(const config::Config& root_config);

 private:
  // Parameter indexing for the modulation table: 6 head params per head,
  // then 8 LFO params per LFO, then dry and wet.
  static constexpr size_t kHeadParamCount = 6;
  static constexpr size_t kLfoParamCount = 8;
  static constexpr size_t kLfoParamBase = kNumHeads * kHeadParamCount;
  static constexpr size_t kMixerParamBase =
      kLfoParamBase + kNumLfos * kLfoParamCount;
  static constexpr size_t kParamCount = kMixerParamBase + 2;

  struct Patch {
    uint16_t slot = 0;  // index into mod_
    uint8_t lfo = 0;
  };

  uint32_t seed_;
  size_t fade_time_;
  bool initialized_ = false;
  Samples<uint32_t> time_ = Samples(0u);

  config::Config base_{};  // sanitized root
  std::array<LFOEngine, kNumLfos> engines_{};
  std::array<float, kNumLfos> anchors_{};  // LFO value meaning "no modulation"

  std::array<Patch, kMaxPatches> patches_{};  // compiled active routes
  size_t patch_count_ = 0;
  uint32_t modulated_heads_ = 0;  // bitmask: heads with a modulated param
  uint32_t modulated_lfos_ = 0;  // bitmask: LFOs whose own params are modulated
  bool mixer_modulated_ = false;

  std::array<float, kParamCount> mod_{};  // summed LFO deltas per param
  std::array<config::Head, kNumHeads> eff_heads_{};  // base + mod, unscaled
  std::array<Fade, kNumHeads> fades_{};
  Frame frame_{};

  config::Config output_config_{};  // lazy materialization for Update()
  bool output_dirty_ = true;

  static std::optional<size_t> ParamSlot(const config::Target& target);
  void Initialize(const config::Config& root_config);
  void CompilePatches();
  void RecomputeMods();
  LfoParams EffectiveLfoParams(size_t lfo_idx) const;
  void UpdateEffectiveHead(size_t head_idx);
  void BeginFades(size_t lfo_idx, const LFOTransition& transition);
  void AddHead(config::Head head, float weight);
  void BuildFrame();
  void AdvanceFades();
  float effective_dry() const;
  float effective_wet() const;
};

}  // namespace fridge::mod

#endif  // FRIDGE_MOD_H_
