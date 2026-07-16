#include "head_transition.hpp"

#include <algorithm>
#include <cmath>

namespace fridge::transition {

const Frame& HeadTransitionMixer::Reset(const config::Config& config) {
  initialized_ = true;
  previous_heads_ = config.heads;
  active_transitions_ = {};
  BuildFrame(config);
  return frame_;
}

const Frame& HeadTransitionMixer::Update(
    const config::Config& config,
    const std::array<std::optional<HeadMotionTransition>, NUM_HEADS>&
        transitions) {
  if (!initialized_) {
    initialized_ = true;
    previous_heads_ = config.heads;
  }

  // Transitions exist to hide clicks/discontinuities in the audio produced by
  // a head when its position jumps. A head that neither reads nor writes
  // makes no such click — so treat those transitions as no-ops. That keeps
  // the steady-state fast path active even when an LFO targeting a
  // read/write-disabled head grain-starts.
  auto head_needs_fade = [&](size_t i) {
    const auto& h = config.heads[i];
    return h.read_amount > 0.f || h.write_amount > 0.f;
  };
  bool any_new_transition = false;
  for (size_t i = 0; i < NUM_HEADS; ++i) {
    if (transitions[i].has_value() && head_needs_fade(i)) {
      any_new_transition = true;
      break;
    }
  }
  bool any_active_transition = false;
  for (const auto& t : active_transitions_) {
    if (t.has_value()) {
      any_active_transition = true;
      break;
    }
  }

  // Steady-state fast path: no transitions in flight and none starting this
  // tick. Skip the full BuildFrame rebuild (zeros 256B + calls ScaleHead which
  // is a no-op at weight=1.0) plus AdvanceTransitions. Called every audio
  // sample, so this ~1k-cycle saving is the difference between fitting in
  // budget and dropping samples.
  if (!any_new_transition && !any_active_transition) {
    frame_.dry = config.dry;
    frame_.wet = config.wet;
    frame_.head_count = NUM_HEADS;
    for (size_t i = 0; i < NUM_HEADS; ++i) {
      frame_.heads[i].head = config.heads[i];
      frame_.heads[i].weight = 1.0f;
    }
    previous_heads_ = config.heads;
    return frame_;
  }

  for (size_t i = 0; i < NUM_HEADS; ++i) {
    if (transitions[i].has_value()) {
      BeginTransition(i, previous_heads_[i], transitions[i].value());
    }
  }

  BuildFrame(config);
  AdvanceTransitions();
  previous_heads_ = config.heads;
  return frame_;
}

void HeadTransitionMixer::BeginTransition(
    size_t head_idx, const config::Head& head,
    const HeadMotionTransition& transition) {
  if (head_idx >= NUM_HEADS ||
      (!transition.reversed && !transition.teleported)) {
    return;
  }
  // Erase-only heads don't produce audio contributions, so a position jump
  // is inaudible — skip the fade to keep them out of active_transitions_
  // (which would otherwise double this head's work in ProcessSample for
  // fade_time_ samples).
  if (head.read_amount == 0.f && head.write_amount == 0.f) {
    return;
  }

  config::Head old_head = head;
  old_head.position = transition.old_motion.position;

  active_transitions_[head_idx] = ActiveTransition{
      .old_head = old_head,
      .old_position = static_cast<float>(transition.old_motion.position),
      .old_direction = transition.old_motion.direction,
      .old_speed = transition.old_motion.speed,
      .samples_remaining = fade_time_,
  };
}

void HeadTransitionMixer::BuildFrame(const config::Config& config) {
  frame_ = Frame{.dry = config.dry, .wet = config.wet};

  for (size_t i = 0; i < NUM_HEADS; ++i) {
    if (active_transitions_[i].has_value()) {
      AddTransitioningHead(i, config.heads[i]);
    } else {
      AddWeightedHead(config.heads[i], 1.0f);
    }
  }
}

void HeadTransitionMixer::AddTransitioningHead(size_t head_idx,
                                               const config::Head& head) {
  ActiveTransition& transition = active_transitions_[head_idx].value();
  float old_weight = static_cast<float>(transition.samples_remaining) /
                     static_cast<float>(fade_time_);
  float new_weight = 1.0f - old_weight;

  config::Head old_head = transition.old_head;
  old_head.position = WrapPosition(transition.old_position);

  AddWeightedHead(old_head, old_weight);
  AddWeightedHead(head, new_weight);
}

void HeadTransitionMixer::AddWeightedHead(config::Head head, float weight) {
  if (frame_.head_count >= frame_.heads.size() || weight <= 0.0f) {
    return;
  }

  frame_.heads[frame_.head_count] = HeadContribution{
      .head = ScaleHead(head, weight),
      .weight = weight,
  };
  frame_.head_count++;
}

void HeadTransitionMixer::AdvanceTransitions() {
  for (std::optional<ActiveTransition>& transition : active_transitions_) {
    if (!transition.has_value()) {
      continue;
    }

    transition->old_position +=
        transition->old_speed * DirectionMultiplier(transition->old_direction);
    transition->samples_remaining--;

    if (transition->samples_remaining == 0) {
      transition = std::nullopt;
    }
  }
}

// ----- Validation

size_t HeadTransitionMixer::ClampFadeTime(size_t fade_time) {
  return std::max<size_t>(1, fade_time);
}

size_t HeadTransitionMixer::WrapPosition(float position) {
  float wrapped = std::fmod(position, static_cast<float>(BUFFER_LEN));
  if (wrapped < 0.0f) {
    wrapped += static_cast<float>(BUFFER_LEN);
  }
  return static_cast<size_t>(std::lround(wrapped)) % BUFFER_LEN;
}

float HeadTransitionMixer::DirectionMultiplier(
    fridge::state::Direction direction) {
  return direction == fridge::state::Direction::kForwards ? 1.0f : -1.0f;
}

config::Head HeadTransitionMixer::ScaleHead(config::Head head, float weight) {
  float clamped_weight = std::clamp(weight, 0.0f, 1.0f);
  head.write_amount *= clamped_weight;
  head.read_amount *= clamped_weight;
  head.erase_amount = 1.0f - ((1.0f - head.erase_amount) * clamped_weight);
  head.feedback.amount *= clamped_weight;
  return head;
}

}  // namespace fridge::transition
