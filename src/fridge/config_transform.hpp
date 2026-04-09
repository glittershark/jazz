#ifndef FRIDGE_CONFIG_TRANSFORM_H_
#define FRIDGE_CONFIG_TRANSFORM_H_

#include <array>
#include <cstdint>
#include <random>

#include "config.hpp"
#include "lfo_engine.hpp"

namespace fridge::transform {

class state {
  uint32_t seed_;
  float time_ = 0.0f;
  bool initialized_ = false;
  config::Config root_config_{};
  config::Config output_config_{};
  std::array<float, NUM_LFOS> lfo_initial_values_{};
  std::array<fridge::state::LFOEngine, NUM_LFOS> lfo_engines_{};

  void Initialize(const config::Config& root_config);
  void RebaseRootConfig(const config::Config& root_config);
  float CurrentDelta(size_t lfo_idx) const;
  void ApplyCurrentDeltas(config::Config& config) const;

  // ----- Validation

  static float ClampFinite(float value, float fallback = 0.0f);
  static float ClampChance(float value);
  static size_t ClampSize(float value, size_t minimum);
  static config::LFO SanitizeLfo(const config::LFO& lfo);
  static config::Config SanitizeConfig(const config::Config& root_config);
  static bool MatchesSanitizedTarget(
      const std::optional<config::Target>& input,
      const std::optional<config::Target>& sanitized);
  static bool MatchesSanitizedLfo(const config::LFO& input,
                                  const config::LFO& sanitized);
  static bool MatchesSanitizedConfig(const config::Config& input,
                                     const config::Config& sanitized);
  static void ApplyTargetDelta(config::Config& config,
                               const config::Target& target, float delta);
  static void ApplyLfoDelta(config::Config& config, size_t lfo_idx,
                            float delta);

 public:
  explicit state(uint32_t seed = std::random_device{}()) : seed_(seed) {}

  const config::Config& Reset(const config::Config& root_config);
  const config::Config& Update(const config::Config& root_config, float dt);
  float time() const { return time_; }
};

}  // namespace fridge::transform

#endif  // FRIDGE_CONFIG_TRANSFORM_H_
