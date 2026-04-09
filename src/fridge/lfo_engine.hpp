#ifndef FRIDGE_LFO_ENGINE_H_
#define FRIDGE_LFO_ENGINE_H_

#include <cstdint>
#include <random>

#include "config.hpp"

namespace fridge::state {

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

}  // namespace fridge::state

#endif  // FRIDGE_LFO_ENGINE_H_
