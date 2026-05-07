#ifndef FRIDGE_HEAD_TRANSITION_H_
#define FRIDGE_HEAD_TRANSITION_H_

#include <array>
#include <cstddef>
#include <optional>

#include "config.hpp"
#include "constants.hpp"
#include "lfo_engine.hpp"

namespace fridge::transition {

struct HeadMotion {
  size_t position = 0;
  fridge::state::Direction direction = fridge::state::Direction::kForwards;
  float speed = 1.0f;
};

struct HeadMotionTransition {
  HeadMotion old_motion{};
  HeadMotion new_motion{};
  bool reversed = false;
  bool teleported = false;
};

struct HeadContribution {
  config::Head head{};
  float weight = 1.0f;
};

struct Frame {
  std::array<HeadContribution, NUM_HEADS * 2> heads{};
  size_t head_count = 0;
  float dry = 1.0f;
  float wet = 1.0f;
};

class HeadTransitionMixer {
 public:
  explicit HeadTransitionMixer(size_t fade_time = FADE_TIME)
      : fade_time_(ClampFadeTime(fade_time)) {}

  const Frame& Reset(const config::Config& config);
  const Frame& Update(const config::Config& config,
                      const std::array<std::optional<HeadMotionTransition>,
                                       NUM_HEADS>& transitions);

 private:
  struct ActiveTransition {
    config::Head old_head{};
    float old_position = 0.0f;
    fridge::state::Direction old_direction =
        fridge::state::Direction::kForwards;
    float old_speed = 1.0f;
    size_t samples_remaining = 0;
  };

  size_t fade_time_ = FADE_TIME;
  bool initialized_ = false;
  Frame frame_{};
  std::array<config::Head, NUM_HEADS> previous_heads_{};
  std::array<std::optional<ActiveTransition>, NUM_HEADS> active_transitions_{};

  void BeginTransition(size_t head_idx, const config::Head& head,
                       const HeadMotionTransition& transition);
  void BuildFrame(const config::Config& config);
  void AddTransitioningHead(size_t head_idx, const config::Head& head);
  void AddWeightedHead(config::Head head, float weight);
  void AdvanceTransitions();

  // ----- Validation

  // Keeps fade math from dividing by zero.
  static size_t ClampFadeTime(size_t fade_time);
  // Wraps transitional head motion back into the sample buffer.
  static size_t WrapPosition(float position);
  // Converts head direction into signed position movement.
  static float DirectionMultiplier(fridge::state::Direction direction);
  // Applies a transition fade weight to a head's audio contributions.
  static config::Head ScaleHead(config::Head head, float weight);
};

}  // namespace fridge::transition

#endif  // FRIDGE_HEAD_TRANSITION_H_
