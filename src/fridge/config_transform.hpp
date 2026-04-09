#ifndef FRIDGE_CONFIG_TRANSFORM_H_
#define FRIDGE_CONFIG_TRANSFORM_H_

#include <array>
#include <cstdint>
#include <random>

#include "config.hpp"
#include "lfo_engine.hpp"

namespace fridge {

class LFOSystem {
  uint32_t seed_;
  float time_ = 0.0f;
  bool initialized_ = false;
  std::array<float, NUM_LFOS> lfo_initial_values_{};
  std::array<state::LFOEngine, NUM_LFOS> lfo_engines_{};

  void Initialize(const config::Config& root_config);
  config::Config BuildVirtualConfig(const config::Config& root_config) const;

 public:
  explicit LFOSystem(uint32_t seed = std::random_device{}()) : seed_(seed) {}

  config::Config Reset(const config::Config& root_config);
  config::Config Update(const config::Config& root_config, float dt);
  float time() const { return time_; }
};

}  // namespace fridge

#endif  // FRIDGE_CONFIG_TRANSFORM_H_
