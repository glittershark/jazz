#ifndef FRIDGE_H_
#define FRIDGE_H_

#include <array>
#include <cstdint>
#include <optional>
#include <random>

#include "config.hpp"

namespace fridge::state {

class VirtualKnob {
  float static_value_ = 0.0f;
  float value_ = 0.0f;

 public:
  VirtualKnob() = default;
  explicit VirtualKnob(float static_value)
      : static_value_(static_value), value_(static_value) {}

  void set_static_value(float static_value) {
    static_value_ = static_value;
    value_ = static_value;
  }

  void Reset() { value_ = static_value_; }
  void AddOffset(float delta) { value_ += delta; }

  float static_value() const { return static_value_; }
  float value() const { return value_; }
};

struct Feedback {
  config::Feedback::Kind kind_ = config::Feedback::Kind::kRead;
  VirtualKnob amount_;

  Feedback() = default;
  explicit Feedback(const config::Feedback& feedback) { Reset(feedback); }

  void Reset(const config::Feedback& feedback) {
    kind_ = feedback.kind();
    amount_.set_static_value(feedback.amount());
  }

  void Reset() { amount_.Reset(); }

  config::Feedback::Kind kind() const { return kind_; }
  VirtualKnob& amount() { return amount_; }
  const VirtualKnob& amount() const { return amount_; }
};

class Head {
  VirtualKnob position_;
  VirtualKnob write_amount_;
  VirtualKnob read_amount_;
  VirtualKnob erase_amount_;
  Feedback feedback_;

 public:
  Head() = default;
  explicit Head(const config::Head& head) { Reset(head); }

  void Reset(const config::Head& head) {
    position_.set_static_value(static_cast<float>(head.position()));
    write_amount_.set_static_value(head.write_amount());
    read_amount_.set_static_value(head.read_amount());
    erase_amount_.set_static_value(head.erase_amount());
    feedback_.Reset(head.feedback());
  }

  void Reset() {
    position_.Reset();
    write_amount_.Reset();
    read_amount_.Reset();
    erase_amount_.Reset();
    feedback_.Reset();
  }

  VirtualKnob& position() { return position_; }
  const VirtualKnob& position() const { return position_; }
  VirtualKnob& write_amount() { return write_amount_; }
  const VirtualKnob& write_amount() const { return write_amount_; }
  VirtualKnob& read_amount() { return read_amount_; }
  const VirtualKnob& read_amount() const { return read_amount_; }
  VirtualKnob& erase_amount() { return erase_amount_; }
  const VirtualKnob& erase_amount() const { return erase_amount_; }
  Feedback& feedback() { return feedback_; }
  const Feedback& feedback() const { return feedback_; }
};

class LFO {
  VirtualKnob range_;
  VirtualKnob max_grain_size_;
  VirtualKnob min_grain_size_;
  VirtualKnob reverse_chance_;
  VirtualKnob teleport_chance_;
  VirtualKnob pitch_shift_chance_;
  VirtualKnob low_octave_chance_;
  VirtualKnob high_octave_chance_;
  std::array<std::optional<config::Target>, MAX_TARGET_PARAMS> targets_{};

 public:
  LFO() = default;
  explicit LFO(const config::LFO& lfo) { Reset(lfo); }

  void Reset(const config::LFO& lfo) {
    range_.set_static_value(static_cast<float>(lfo.range()));
    max_grain_size_.set_static_value(static_cast<float>(lfo.max_grain_size()));
    min_grain_size_.set_static_value(static_cast<float>(lfo.min_grain_size()));
    reverse_chance_.set_static_value(lfo.reverse_chance());
    teleport_chance_.set_static_value(lfo.teleport_chance());
    pitch_shift_chance_.set_static_value(lfo.pitch_shift_chance());
    low_octave_chance_.set_static_value(lfo.low_octave_chance());
    high_octave_chance_.set_static_value(lfo.high_octave_chance());
    targets_ = lfo.targets();
  }

  void Reset() {
    range_.Reset();
    max_grain_size_.Reset();
    min_grain_size_.Reset();
    reverse_chance_.Reset();
    teleport_chance_.Reset();
    pitch_shift_chance_.Reset();
    low_octave_chance_.Reset();
    high_octave_chance_.Reset();
  }

  VirtualKnob& range() { return range_; }
  const VirtualKnob& range() const { return range_; }
  VirtualKnob& max_grain_size() { return max_grain_size_; }
  const VirtualKnob& max_grain_size() const { return max_grain_size_; }
  VirtualKnob& min_grain_size() { return min_grain_size_; }
  const VirtualKnob& min_grain_size() const { return min_grain_size_; }
  VirtualKnob& reverse_chance() { return reverse_chance_; }
  const VirtualKnob& reverse_chance() const { return reverse_chance_; }
  VirtualKnob& teleport_chance() { return teleport_chance_; }
  const VirtualKnob& teleport_chance() const { return teleport_chance_; }
  VirtualKnob& pitch_shift_chance() { return pitch_shift_chance_; }
  const VirtualKnob& pitch_shift_chance() const { return pitch_shift_chance_; }
  VirtualKnob& low_octave_chance() { return low_octave_chance_; }
  const VirtualKnob& low_octave_chance() const { return low_octave_chance_; }
  VirtualKnob& high_octave_chance() { return high_octave_chance_; }
  const VirtualKnob& high_octave_chance() const { return high_octave_chance_; }
  std::array<std::optional<config::Target>, MAX_TARGET_PARAMS>& targets() {
    return targets_;
  }
  const std::array<std::optional<config::Target>, MAX_TARGET_PARAMS>& targets()
      const {
    return targets_;
  }
};

enum class Direction { kForwards = 1, kBackwards = -1 };

class LFOEngine {
  config::LFO config_{};
  std::mt19937 rng_;
  float value_ = 0.0f;
  float speed_ = 1.0f;
  float grain_time_remaining_ = 0.0f;
  size_t grain_size_ = 0;
  Direction direction_ = Direction::kForwards;

  void StartNewGrain(bool initial_grain);
  size_t SampleGrainSize();
  float SampleSpeed();
  bool RollChance(float chance);
  float SampleTeleportValue();

 public:
  LFOEngine() = default;
  explicit LFOEngine(const config::LFO& config);
  LFOEngine(const config::LFO& config, uint32_t seed);

  void SetConfig(const config::LFO& config);
  void Reset(float initial_value = 0.0f,
             Direction direction = Direction::kForwards);
  float Tick(float dt);

  const config::LFO& config() const { return config_; }
  float value() const { return value_; }
  float speed() const { return speed_; }
  float grain_time_remaining() const { return grain_time_remaining_; }
  size_t grain_size() const { return grain_size_; }
  Direction direction() const { return direction_; }
};

class Config {
  struct TargetBinding {
    VirtualKnob* knob = nullptr;
  };

  std::array<Head, NUM_HEADS> heads_{};
  std::array<LFO, NUM_LFOS> lfos_{};
  VirtualKnob dry_{1.0f};
  VirtualKnob wet_{1.0f};
  std::array<LFOEngine, NUM_LFOS> lfo_engines_{};
  std::array<float, NUM_LFOS> lfo_initial_values_{};
  std::array<std::array<TargetBinding, MAX_TARGET_PARAMS>, NUM_LFOS>
      target_bindings_{};
  uint32_t seed_ = 0;
  float time_ = 0.0f;

  config::LFO SnapshotLFOConfig(size_t lfo_idx) const;
  VirtualKnob* ResolveTarget(const config::Target& target);
  void ResolveBindings();
  void ResetVirtualValues();
  void ApplyModulations();

 public:
  Config() = default;
  explicit Config(const config::Config& config,
                  uint32_t seed = std::random_device{}()) {
    Load(config, seed);
  }

  void Load(const config::Config& config,
            uint32_t seed = std::random_device{}());
  void Reset();
  void Tick(float dt);

  std::array<Head, NUM_HEADS>& heads() { return heads_; }
  const std::array<Head, NUM_HEADS>& heads() const { return heads_; }
  std::array<LFO, NUM_LFOS>& lfos() { return lfos_; }
  const std::array<LFO, NUM_LFOS>& lfos() const { return lfos_; }
  VirtualKnob& dry() { return dry_; }
  const VirtualKnob& dry() const { return dry_; }
  VirtualKnob& wet() { return wet_; }
  const VirtualKnob& wet() const { return wet_; }
  const std::array<LFOEngine, NUM_LFOS>& lfo_engines() const {
    return lfo_engines_;
  }
  float time() const { return time_; }
};

}  // namespace fridge::state

#endif  // FRIDGE_H_
