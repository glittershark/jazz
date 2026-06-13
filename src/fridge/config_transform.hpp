#ifndef FRIDGE_CONFIG_TRANSFORM_H_
#define FRIDGE_CONFIG_TRANSFORM_H_

#include <array>
#include <cstdint>
#include <optional>
#include <random>

#include "config.hpp"
#include "head_transition.hpp"
#include "lfo_engine.hpp"
#include "libjazz/units.hpp"

namespace fridge::transform {

using namespace jazz::units;

class State {
  uint32_t seed_;
  Samples<uint32_t> time_ = Samples(0);
  bool initialized_ = false;
  config::Config root_config_{};
  config::Config output_config_{};
  std::array<float, NUM_LFOS> lfo_initial_values_{};
  std::array<fridge::state::LFOEngine, NUM_LFOS> lfo_engines_{};
  std::array<std::optional<fridge::transition::HeadMotionTransition>, NUM_HEADS>
      head_transitions_{};

  void Initialize(const config::Config& root_config);
  void RebaseRootConfig(const config::Config& root_config);
  float CurrentDelta(size_t lfo_idx) const;
  void ApplyCurrentDeltas(config::Config& config) const;
  void RecordHeadTransitions(size_t lfo_idx,
                             const fridge::state::LFOTransition& transition,
                             const config::Config& previous_output);

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
  explicit State(uint32_t seed = std::random_device{}()) : seed_(seed) {}

  const config::Config& Reset(const config::Config& root_config);
  const config::Config& Update(const config::Config& root_config,
                               Samples<uint32_t> step);
  Samples<uint32_t> time() const { return time_; }
  const std::array<std::optional<fridge::transition::HeadMotionTransition>,
                   NUM_HEADS>&
  head_transitions() const {
    return head_transitions_;
  }
};

}  // namespace fridge::transform

#endif  // FRIDGE_CONFIG_TRANSFORM_H_
