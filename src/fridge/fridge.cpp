#include "fridge.hpp"

#include <algorithm>
#include <cmath>

namespace {

float DirectionMultiplier(fridge::state::Direction direction) {
  return direction == fridge::state::Direction::kForwards ? 1.0f : -1.0f;
}

fridge::state::Direction ReverseDirection(fridge::state::Direction direction) {
  return direction == fridge::state::Direction::kForwards
             ? fridge::state::Direction::kBackwards
             : fridge::state::Direction::kForwards;
}

float ClampChance(float value) {
  if (!std::isfinite(value)) {
    return 0.0f;
  }

  return std::clamp(value, 0.0f, 1.0f);
}

size_t ClampSize(float value, size_t minimum) {
  if (!std::isfinite(value)) {
    return minimum;
  }

  return static_cast<size_t>(
      std::max<float>(static_cast<float>(minimum), std::lround(value)));
}

} // namespace

namespace fridge::state {

LFOEngine::LFOEngine(const config::LFO &config)
    : LFOEngine(config, std::random_device{}()) {}

LFOEngine::LFOEngine(const config::LFO &config, uint32_t seed)
    : config_(config), rng_(seed) {
  Reset();
}

void LFOEngine::SetConfig(const config::LFO &config) {
  config_ = config;
  value_ = std::clamp(value_, 0.0f, static_cast<float>(config_.range()));
}

void LFOEngine::Reset(float initial_value, Direction direction) {
  value_ =
      std::clamp(initial_value, 0.0f, static_cast<float>(config_.range()));
  direction_ = direction;
  StartNewGrain(true);
}

float LFOEngine::Tick(float dt) {
  float remaining = std::max(0.0f, dt);

  while (remaining > 0.0f) {
    if (grain_time_remaining_ <= 0.0f) {
      StartNewGrain(false);
    }

    float slice = std::min(remaining, grain_time_remaining_);
    value_ = std::clamp(value_ + (speed_ * DirectionMultiplier(direction_) *
                                  slice),
                        0.0f, static_cast<float>(config_.range()));
    grain_time_remaining_ -= slice;
    remaining -= slice;

    if (grain_time_remaining_ <= 0.0f) {
      StartNewGrain(false);
    }
  }

  return value_;
}

void LFOEngine::StartNewGrain(bool initial_grain) {
  if (!initial_grain) {
    if (RollChance(config_.reverse_chance())) {
      direction_ = ReverseDirection(direction_);
    }

    if (RollChance(config_.teleport_chance())) {
      value_ = SampleTeleportValue();
    }
  }

  grain_size_ = SampleGrainSize();
  speed_ = SampleSpeed();
  grain_time_remaining_ = static_cast<float>(grain_size_);
}

size_t LFOEngine::SampleGrainSize() {
  size_t min_grain_size =
      std::max<size_t>(1, std::min(config_.min_grain_size(),
                                   config_.max_grain_size()));
  size_t max_grain_size =
      std::max(min_grain_size,
               std::max(config_.min_grain_size(), config_.max_grain_size()));

  std::uniform_int_distribution<size_t> dist(min_grain_size, max_grain_size);
  return dist(rng_);
}

float LFOEngine::SampleSpeed() {
  if (!RollChance(config_.pitch_shift_chance())) {
    return 1.0f;
  }

  float low = std::max(0.0f, config_.low_octave_chance());
  float high = std::max(0.0f, config_.high_octave_chance());
  float total = low + high;

  if (total <= 0.0f) {
    return 1.0f;
  }

  std::bernoulli_distribution high_dist(high / total);
  return high_dist(rng_) ? 2.0f : 0.5f;
}

bool LFOEngine::RollChance(float chance) {
  std::bernoulli_distribution dist(std::clamp(chance, 0.0f, 1.0f));
  return dist(rng_);
}

float LFOEngine::SampleTeleportValue() {
  std::uniform_real_distribution<float> dist(0.0f,
                                             static_cast<float>(config_.range()));
  return dist(rng_);
}

void Config::Load(const config::Config &config, uint32_t seed) {
  seed_ = seed;

  for (size_t i = 0; i < NUM_HEADS; ++i) {
    heads_[i].Reset(config.heads()[i]);
  }

  for (size_t i = 0; i < NUM_LFOS; ++i) {
    lfos_[i].Reset(config.lfos()[i]);
  }

  dry_.set_static_value(config.dry());
  wet_.set_static_value(config.wet());

  ResolveBindings();
  Reset();
}

void Config::Reset() {
  time_ = 0.0f;
  ResetVirtualValues();

  for (size_t i = 0; i < NUM_LFOS; ++i) {
    config::LFO lfo_config = SnapshotLFOConfig(i);
    lfo_engines_[i] = LFOEngine(lfo_config, seed_ + static_cast<uint32_t>(i));
    lfo_engines_[i].Reset(static_cast<float>(lfo_config.range()) * 0.5f,
                          Direction::kForwards);
    lfo_initial_values_[i] = lfo_engines_[i].value();
  }

  ApplyModulations();
}

void Config::Tick(float dt) {
  float step = std::max(0.0f, dt);
  if (step <= 0.0f) {
    return;
  }

  for (size_t i = 0; i < NUM_LFOS; ++i) {
    lfo_engines_[i].SetConfig(SnapshotLFOConfig(i));
  }

  for (size_t i = 0; i < NUM_LFOS; ++i) {
    lfo_engines_[i].Tick(step);
  }

  time_ += step;
  ResetVirtualValues();
  ApplyModulations();
}

config::LFO Config::SnapshotLFOConfig(size_t lfo_idx) const {
  const LFO &lfo = lfos_[lfo_idx];

  config::LFO snapshot(
      ClampSize(lfo.range().value(), 0), ClampSize(lfo.max_grain_size().value(), 1),
      ClampSize(lfo.min_grain_size().value(), 1),
      ClampChance(lfo.reverse_chance().value()),
      ClampChance(lfo.teleport_chance().value()),
      ClampChance(lfo.pitch_shift_chance().value()),
      ClampChance(lfo.low_octave_chance().value()),
      ClampChance(lfo.high_octave_chance().value()));
  snapshot.targets() = lfo.targets();

  return snapshot;
}

VirtualKnob *Config::ResolveTarget(const config::Target &target) {
  using config::TargetObject;
  using config::TargetParameter;

  if (target.object() == TargetObject::kHead) {
    if (target.object_idx() >= NUM_HEADS) {
      return nullptr;
    }

    Head &head = heads_[target.object_idx()];
    switch (target.parameter()) {
    case TargetParameter::kPosition:
      return &head.position();
    case TargetParameter::kWriteAmount:
      return &head.write_amount();
    case TargetParameter::kReadAmount:
      return &head.read_amount();
    case TargetParameter::kEraseAmount:
      return &head.erase_amount();
    case TargetParameter::kFeedbackAmount:
      return &head.feedback().amount();
    default:
      return nullptr;
    }
  }

  if (target.object() == TargetObject::kLFO) {
    if (target.object_idx() >= NUM_LFOS) {
      return nullptr;
    }

    LFO &lfo = lfos_[target.object_idx()];
    switch (target.parameter()) {
    case TargetParameter::kRange:
      return &lfo.range();
    case TargetParameter::kMaxGrainSize:
      return &lfo.max_grain_size();
    case TargetParameter::kMinGrainSize:
      return &lfo.min_grain_size();
    case TargetParameter::kReverseChance:
      return &lfo.reverse_chance();
    case TargetParameter::kTeleportChance:
      return &lfo.teleport_chance();
    case TargetParameter::kPitchShiftChance:
      return &lfo.pitch_shift_chance();
    case TargetParameter::kLowOctaveChance:
      return &lfo.low_octave_chance();
    case TargetParameter::kHighOctaveChance:
      return &lfo.high_octave_chance();
    default:
      return nullptr;
    }
  }

  if (target.object() != TargetObject::kMixer) {
    return nullptr;
  }

  switch (target.parameter()) {
  case TargetParameter::kDry:
    return &dry_;
  case TargetParameter::kWet:
    return &wet_;
  default:
    return nullptr;
  }
}

void Config::ResolveBindings() {
  for (size_t i = 0; i < NUM_LFOS; ++i) {
    for (size_t target_idx = 0; target_idx < MAX_TARGET_PARAMS; ++target_idx) {
      target_bindings_[i][target_idx].knob = nullptr;
      if (lfos_[i].targets()[target_idx].has_value()) {
        target_bindings_[i][target_idx].knob =
            ResolveTarget(lfos_[i].targets()[target_idx].value());
      }
    }
  }
}

void Config::ResetVirtualValues() {
  for (Head &head : heads_) {
    head.Reset();
  }

  for (LFO &lfo : lfos_) {
    lfo.Reset();
  }

  dry_.Reset();
  wet_.Reset();
}

void Config::ApplyModulations() {
  for (size_t i = 0; i < NUM_LFOS; ++i) {
    float delta = lfo_engines_[i].value() - lfo_initial_values_[i];

    for (size_t target_idx = 0; target_idx < MAX_TARGET_PARAMS; ++target_idx) {
      if (target_bindings_[i][target_idx].knob != nullptr) {
        target_bindings_[i][target_idx].knob->AddOffset(delta);
      }
    }
  }
}

} // namespace fridge::state

#ifndef UNIT_TEST

#include "daisy_seed.h"

daisy::DaisySeed hw;

int main(void) {
  hw.Init();
  hw.SetAudioBlockSize(8);
  hw.StartLog();
}

#endif
