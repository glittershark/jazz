#include <algorithm>
#include <cmath>

#include "lfo_engine.hpp"

namespace {

float DirectionMultiplier(fridge::state::Direction direction) {
  return direction == fridge::state::Direction::kForwards ? 1.0f : -1.0f;
}

fridge::state::Direction ReverseDirection(fridge::state::Direction direction) {
  return direction == fridge::state::Direction::kForwards
             ? fridge::state::Direction::kBackwards
             : fridge::state::Direction::kForwards;
}

}  // namespace

namespace fridge::state {

LFOEngine::LFOEngine(const config::LFO& config)
    : LFOEngine(config, std::random_device{}()) {}

LFOEngine::LFOEngine(const config::LFO& config, uint32_t seed)
    : config_(config), rng_(seed) {
  Reset();
}

void LFOEngine::SetConfig(const config::LFO& config) {
  config_ = config;
  value_ = std::clamp(value_, 0.0f, static_cast<float>(config_.range));
}

void LFOEngine::Reset(float initial_value, Direction direction) {
  value_ = std::clamp(initial_value, 0.0f, static_cast<float>(config_.range));
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
    value_ =
        std::clamp(value_ + (speed_ * DirectionMultiplier(direction_) * slice),
                   0.0f, static_cast<float>(config_.range));
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
    if (RollChance(config_.reverse_chance)) {
      direction_ = ReverseDirection(direction_);
    }

    if (RollChance(config_.teleport_chance)) {
      value_ = SampleTeleportValue();
    }
  }

  grain_size_ = SampleGrainSize();
  speed_ = SampleSpeed();
  grain_time_remaining_ = static_cast<float>(grain_size_);
}

size_t LFOEngine::SampleGrainSize() {
  size_t min_grain_size = std::max<size_t>(
      1, std::min(config_.min_grain_size, config_.max_grain_size));
  size_t max_grain_size =
      std::max(min_grain_size,
               std::max(config_.min_grain_size, config_.max_grain_size));

  std::uniform_int_distribution<size_t> dist(min_grain_size, max_grain_size);
  return dist(rng_);
}

float LFOEngine::SampleSpeed() {
  if (!RollChance(config_.pitch_shift_chance)) {
    return 1.0f;
  }

  float low = std::max(0.0f, config_.low_octave_chance);
  float high = std::max(0.0f, config_.high_octave_chance);
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
                                             static_cast<float>(config_.range));
  return dist(rng_);
}

}  // namespace fridge::state
