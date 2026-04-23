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

float WrapToRange(float value, size_t range) {
  const float upper_bound = static_cast<float>(range);
  if (upper_bound <= 0.0f) {
    return 0.0f;
  }

  float wrapped = std::fmod(value, upper_bound);
  if (wrapped < 0.0f) {
    wrapped += upper_bound;
  }
  return wrapped;
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
  value_ = WrapToRange(value_, config_.range);
}

void LFOEngine::Reset(float initial_value, Direction direction) {
  value_ = WrapToRange(initial_value, config_.range);
  direction_ = direction;
  StartNewGrain(true);
}

float LFOEngine::Tick(float dt) {
  LFOTickResult result = TickWithEvents(dt);
  return result.value;
}

LFOTickResult LFOEngine::TickWithEvents(float dt) {
  LFOTickResult result{.value = value_};
  float remaining = std::max(0.0f, dt);

  while (remaining > 0.0f) {
    if (grain_time_remaining_ <= 0.0f) {
      std::optional<LFOTransition> transition = StartNewGrain(false);
      if (transition.has_value()) {
        result.transition = transition;
      }
    }

    float slice = std::min(remaining, grain_time_remaining_);
    value_ =
        WrapToRange(value_ + (speed_ * DirectionMultiplier(direction_) * slice),
                    config_.range);
    grain_time_remaining_ -= slice;
    remaining -= slice;

    if (grain_time_remaining_ <= 0.0f) {
      std::optional<LFOTransition> transition = StartNewGrain(false);
      if (transition.has_value()) {
        if (result.transition.has_value()) {
          result.transition->new_value = transition->new_value;
          result.transition->new_direction = transition->new_direction;
          result.transition->new_speed = transition->new_speed;
          result.transition->reversed =
              result.transition->reversed || transition->reversed;
          result.transition->teleported =
              result.transition->teleported || transition->teleported;
        } else {
          result.transition = transition;
        }
      }
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
    if (RollChance(config_.reverse_chance)) {
      direction_ = ReverseDirection(direction_);
      transition.reversed = true;
    }

    if (RollChance(config_.teleport_chance)) {
      value_ = SampleTeleportValue();
      transition.teleported = true;
    }
  }

  grain_size_ = SampleGrainSize();
  speed_ = SampleSpeed();
  grain_time_remaining_ = static_cast<float>(grain_size_);

  if (!transition.reversed && !transition.teleported) {
    return std::nullopt;
  }

  transition.new_value = value_;
  transition.new_direction = direction_;
  transition.new_speed = speed_;
  return transition;
}

size_t LFOEngine::SampleGrainSize() {
  size_t min_grain_size = std::max<size_t>(
      1, std::min(config_.min_grain_size, config_.max_grain_size));
  size_t max_grain_size = std::max(
      {min_grain_size, config_.min_grain_size, config_.max_grain_size});

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
