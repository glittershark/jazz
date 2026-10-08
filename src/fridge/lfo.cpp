#include "lfo.hpp"

#include <algorithm>
#include <cmath>

namespace fridge::mod {

namespace {

float DirectionMultiplier(Direction direction) {
  return direction == Direction::kForwards ? 1.0f : -1.0f;
}

Direction ReverseDirection(Direction direction) {
  return direction == Direction::kForwards ? Direction::kBackwards
                                           : Direction::kForwards;
}

/* When several grain boundaries land inside one tick, report a single
 * transition spanning from the motion before the first to the motion after
 * the last. */
LFOTransition ExtendTransition(const LFOTransition& accumulated,
                               const LFOTransition& latest) {
  LFOTransition transition = latest;
  transition.old_value = accumulated.old_value;
  transition.old_direction = accumulated.old_direction;
  transition.old_speed = accumulated.old_speed;
  transition.reversed = accumulated.reversed || latest.reversed;
  transition.teleported = accumulated.teleported || latest.teleported;
  return transition;
}

void MergeTransition(LFOTickResult& result,
                     std::optional<LFOTransition> transition) {
  if (!transition.has_value()) {
    return;
  }
  if (result.transition.has_value()) {
    result.transition = ExtendTransition(*result.transition, *transition);
  } else {
    result.transition = transition;
  }
}

}  // namespace

// ----- Rng

Rng::Rng(uint32_t seed) {
  // splitmix-style scramble so nearby seeds diverge; xorshift state must be
  // nonzero
  uint32_t z = seed + 0x9E3779B9u;
  z = (z ^ (z >> 16)) * 0x85EBCA6Bu;
  z = (z ^ (z >> 13)) * 0xC2B2AE35u;
  z ^= z >> 16;
  state_ = z != 0 ? z : 0xA341316Cu;
}

uint32_t Rng::Next() {
  uint32_t x = state_;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return state_ = x;
}

float Rng::NextFloat() {
  return static_cast<float>(Next() >> 8) * 0x1.0p-24f;
}

bool Rng::Chance(float chance) {
  if (chance <= 0.0f) {
    return false;
  }
  if (chance >= 1.0f) {
    return true;
  }
  return NextFloat() < chance;
}

uint32_t Rng::Between(uint32_t lo, uint32_t hi) {
  return hi > lo ? lo + Next() % (hi - lo + 1) : lo;
}

// ----- LFOEngine

LFOEngine::LFOEngine(const config::LFO& config) : LFOEngine(config, 1) {}

LFOEngine::LFOEngine(const config::LFO& config, uint32_t seed) : rng_(seed) {
  SetConfig(config);
  Reset();
}

void LFOEngine::SetParams(const LfoParams& params, bool preserve_phase) {
  if (preserve_phase && params_.range != 0) {
    value_ *= static_cast<float>(params.range) / params_.range;
  }
  params_ = params;
  value_ = Wrap(value_);
}

void LFOEngine::SetConfig(const config::LFO& config) {
  SetParams(LfoParams{
      .range = config.range,
      .max_grain_size = config.max_grain_size,
      .min_grain_size = config.min_grain_size,
      .reverse_chance = config.reverse_chance,
      .teleport_chance = config.teleport_chance,
      .pitch_shift_chance = config.pitch_shift_chance,
      .low_octave_chance = config.low_octave_chance,
      .high_octave_chance = config.high_octave_chance,
  });
}

config::LFO LFOEngine::config() const {
  return config::LFO{
      .range = params_.range,
      .max_grain_size = params_.max_grain_size,
      .min_grain_size = params_.min_grain_size,
      .reverse_chance = params_.reverse_chance,
      .teleport_chance = params_.teleport_chance,
      .pitch_shift_chance = params_.pitch_shift_chance,
      .low_octave_chance = params_.low_octave_chance,
      .high_octave_chance = params_.high_octave_chance,
  };
}

void LFOEngine::Reset(float initial_value, Direction direction) {
  value_ = Wrap(initial_value);
  direction_ = direction;
  StartNewGrain(true);
}

float LFOEngine::Wrap(float value) const {
  if (params_.range == 0) {
    return 0.0f;
  }
  const float range = static_cast<float>(params_.range);
  if (value >= range) {
    value -= range;
    if (value >= range) {
      value = std::fmod(value, range);
    }
  } else if (value < 0.0f) {
    value += range;
    if (value < 0.0f) {
      value = std::fmod(value, range);
      if (value < 0.0f) {
        value += range;
      }
    }
  }
  return value;
}

float LFOEngine::Tick(Samples<uint32_t> step) {
  return TickWithEvents(step).value;
}

LFOTickResult LFOEngine::TickWithEvents(Samples<uint32_t> step) {
  LFOTickResult result{.value = value_};
  uint32_t remaining = step.samples();

  while (remaining > 0) {
    if (grain_remaining_ == 0) {
      MergeTransition(result, StartNewGrain(false));
    }

    const uint32_t hop = std::min(remaining, grain_remaining_);
    value_ = Wrap(value_ + speed_ * DirectionMultiplier(direction_) *
                               static_cast<float>(hop));
    grain_remaining_ -= hop;
    remaining -= hop;

    if (grain_remaining_ == 0) {
      MergeTransition(result, StartNewGrain(false));
    }
  }

  result.value = value_;
  return result;
}

std::optional<LFOTransition> LFOEngine::StartNewGrain(bool initial_grain) {
  LFOTransition transition{
      .old_value = value_,
      .old_direction = direction_,
      .old_speed = speed_,
  };

  if (!initial_grain) {
    if (rng_.Chance(params_.reverse_chance)) {
      direction_ = ReverseDirection(direction_);
      transition.reversed = true;
    }

    if (rng_.Chance(params_.teleport_chance)) {
      value_ = rng_.NextFloat() * static_cast<float>(params_.range);
      transition.teleported = true;
    }
  }

  grain_size_ = SampleGrainSize();
  speed_ = SampleSpeed();
  grain_remaining_ = grain_size_;

  if (!transition.reversed && !transition.teleported) {
    return std::nullopt;
  }

  transition.new_value = value_;
  transition.new_direction = direction_;
  transition.new_speed = speed_;
  return transition;
}

uint32_t LFOEngine::SampleGrainSize() {
  const uint32_t lo = static_cast<uint32_t>(std::max<size_t>(
      1, std::min(params_.min_grain_size, params_.max_grain_size)));
  const uint32_t hi = static_cast<uint32_t>(
      std::max<size_t>({lo, params_.min_grain_size, params_.max_grain_size}));
  return rng_.Between(lo, hi);
}

float LFOEngine::SampleSpeed() {
  if (!rng_.Chance(params_.pitch_shift_chance)) {
    return 1.0f;
  }

  const float low = std::max(0.0f, params_.low_octave_chance);
  const float high = std::max(0.0f, params_.high_octave_chance);
  const float total = low + high;

  if (total <= 0.0f) {
    return 1.0f;
  }

  return rng_.Chance(high / total) ? 2.0f : 0.5f;
}

}  // namespace fridge::mod
