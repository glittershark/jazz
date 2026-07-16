#include "config_transform.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>

#include "constants.hpp"

namespace fridge::transform {

// ----- Validation

float ClampFinite(float value, float fallback = 0.0f) {
  return std::isfinite(value) ? value : fallback;
}

float ClampChance(float value) {
  if (!std::isfinite(value)) {
    return 0.0f;
  }

  return std::clamp(value, 0.0f, 1.0f);
}

config::LFO State::SanitizeLfo(const config::LFO& lfo) {
  config::LFO sanitized{
      .range = std::max<size_t>(lfo.range, 0),
      .max_grain_size = std::max<size_t>(lfo.max_grain_size, 1),
      .min_grain_size = std::max<size_t>(lfo.min_grain_size, 1),
      .reverse_chance = ClampChance(lfo.reverse_chance),
      .teleport_chance = ClampChance(lfo.teleport_chance),
      .pitch_shift_chance = ClampChance(lfo.pitch_shift_chance),
      .low_octave_chance = ClampChance(lfo.low_octave_chance),
      .high_octave_chance = ClampChance(lfo.high_octave_chance),
  };
  sanitized.targets = lfo.targets;
  return sanitized;
}

config::Config State::SanitizeConfig(const config::Config& root_config) {
  config::Config sanitized = root_config;

  for (auto& head : sanitized.heads) {
    head.write_amount = ClampFinite(head.write_amount);
    head.read_amount = ClampFinite(head.read_amount);
    head.erase_amount = ClampFinite(head.erase_amount);
    head.feedback.amount = ClampFinite(head.feedback.amount);
  }

  for (size_t i = 0; i < NUM_LFOS; ++i) {
    sanitized.lfos[i] = SanitizeLfo(sanitized.lfos[i]);
  }

  sanitized.dry = ClampFinite(sanitized.dry);
  sanitized.wet = ClampFinite(sanitized.wet);
  return sanitized;
}

bool State::MatchesSanitizedTarget(
    const std::optional<config::Target>& input,
    const std::optional<config::Target>& sanitized) {
  if (input.has_value() != sanitized.has_value()) {
    return false;
  }

  if (!input.has_value()) {
    return true;
  }

  return input->object == sanitized->object &&
         input->parameter == sanitized->parameter &&
         input->object_idx == sanitized->object_idx;
}

bool State::MatchesSanitizedLfo(const config::LFO& input,
                                const config::LFO& sanitized) {
  if (std::max<size_t>(input.range, 0) != sanitized.range ||
      std::max<size_t>(input.max_grain_size, 1) != sanitized.max_grain_size ||
      std::max<size_t>(input.min_grain_size, 1) != sanitized.min_grain_size ||
      ClampChance(input.reverse_chance) != sanitized.reverse_chance ||
      ClampChance(input.teleport_chance) != sanitized.teleport_chance ||
      ClampChance(input.pitch_shift_chance) != sanitized.pitch_shift_chance ||
      ClampChance(input.low_octave_chance) != sanitized.low_octave_chance ||
      ClampChance(input.high_octave_chance) != sanitized.high_octave_chance) {
    return false;
  }

  for (size_t i = 0; i < MAX_TARGET_PARAMS; ++i) {
    if (!MatchesSanitizedTarget(input.targets[i], sanitized.targets[i])) {
      return false;
    }
  }

  return true;
}

bool State::MatchesSanitizedConfig(const config::Config& input,
                                   const config::Config& sanitized) {
  for (size_t i = 0; i < NUM_HEADS; ++i) {
    const config::Head& input_head = input.heads[i];
    const config::Head& sanitized_head = sanitized.heads[i];
    if (input_head.position != sanitized_head.position ||
        ClampFinite(input_head.write_amount) != sanitized_head.write_amount ||
        ClampFinite(input_head.read_amount) != sanitized_head.read_amount ||
        ClampFinite(input_head.erase_amount) != sanitized_head.erase_amount ||
        input_head.feedback.kind != sanitized_head.feedback.kind ||
        ClampFinite(input_head.feedback.amount) !=
            sanitized_head.feedback.amount) {
      return false;
    }
  }

  for (size_t i = 0; i < NUM_LFOS; ++i) {
    if (!MatchesSanitizedLfo(input.lfos[i], sanitized.lfos[i])) {
      return false;
    }
  }

  return ClampFinite(input.dry) == sanitized.dry &&
         ClampFinite(input.wet) == sanitized.wet;
}

void State::ApplyTargetDelta(config::Config& config,
                             const config::Target& target, float delta) {
  using config::TargetObject;
  using config::TargetParameter;

  if (target.object == TargetObject::kHead) {
    if (target.object_idx >= NUM_HEADS) {
      return;
    }

    config::Head& head = config.heads[target.object_idx];
    switch (target.parameter) {
      // TODO(aspen): All non-position heads should be divided by BUFFER_LEN
    case TargetParameter::kPosition:
      head.position =
          std::max<size_t>(head.position + delta + BUFFER_LEN, 0) % BUFFER_LEN;
      return;
    case TargetParameter::kWriteAmount:
      head.write_amount = ClampFinite(head.write_amount + delta);
      return;
    case TargetParameter::kReadAmount:
      head.read_amount = ClampFinite(head.read_amount + delta);
      return;
    case TargetParameter::kEraseAmount:
      head.erase_amount = ClampFinite(head.erase_amount + delta);
      return;
    case TargetParameter::kFeedbackAmount:
      head.feedback.amount = ClampFinite(head.feedback.amount + delta);
      return;
    case TargetParameter::kPan:
      head.pan.pan() = ClampFinite(head.pan.pan() + delta);
      return;
    default:
      return;
    }
  }

  if (target.object == TargetObject::kLFO) {
    if (target.object_idx >= NUM_LFOS) {
      return;
    }

    config::LFO& lfo = config.lfos[target.object_idx];
    switch (target.parameter) {
    case TargetParameter::kRange:
      lfo.range = std::max<size_t>(lfo.range + delta, 0);
      return;
    case TargetParameter::kMaxGrainSize:
      lfo.max_grain_size = std::max<size_t>(lfo.max_grain_size + delta, 1);
      return;
    case TargetParameter::kMinGrainSize:
      lfo.min_grain_size = std::max<size_t>(lfo.min_grain_size + delta, 1);
      return;
    case TargetParameter::kReverseChance:
      lfo.reverse_chance = ClampChance(lfo.reverse_chance + delta);
      return;
    case TargetParameter::kTeleportChance:
      lfo.teleport_chance = ClampChance(lfo.teleport_chance + delta);
      return;
    case TargetParameter::kPitchShiftChance:
      lfo.pitch_shift_chance = ClampChance(lfo.pitch_shift_chance + delta);
      return;
    case TargetParameter::kLowOctaveChance:
      lfo.low_octave_chance = ClampChance(lfo.low_octave_chance + delta);
      return;
    case TargetParameter::kHighOctaveChance:
      lfo.high_octave_chance = ClampChance(lfo.high_octave_chance + delta);
      return;
    default:
      return;
    }
  }

  if (target.object != TargetObject::kMixer) {
    return;
  }

  switch (target.parameter) {
  case TargetParameter::kDry:
    config.dry = ClampFinite(config.dry + delta);
    return;
  case TargetParameter::kWet:
    config.wet = ClampFinite(config.wet + delta);
    return;
  default:
    return;
  }
}

void State::ApplyLfoDelta(config::Config& config, size_t lfo_idx, float delta) {
  if (lfo_idx >= NUM_LFOS || delta == 0.0f) {
    return;
  }

  for (size_t target_idx = 0; target_idx < MAX_TARGET_PARAMS; ++target_idx) {
    if (config.lfos[lfo_idx].targets[target_idx].has_value()) {
      ApplyTargetDelta(config, config.lfos[lfo_idx].targets[target_idx].value(),
                       delta);
    }
  }
}

void State::Initialize(const config::Config& root_config) {
  time_ = Samples(0);
  initialized_ = true;
  head_transitions_ = {};
  root_config_ = SanitizeConfig(root_config);
  output_config_ = root_config_;

  for (size_t i = 0; i < NUM_LFOS; ++i) {
    config::LFO lfo_config = root_config_.lfos[i];
    lfo_engines_[i] =
        fridge::state::LFOEngine(lfo_config, seed_ + static_cast<uint32_t>(i));
    lfo_engines_[i].Reset(0.0f, fridge::state::Direction::kForwards);
    lfo_initial_values_[i] = lfo_engines_[i].value();
  }
}

void State::ApplyCurrentDeltas(config::Config& config) const {
  for (size_t i = 0; i < NUM_LFOS; ++i) {
    ApplyLfoDelta(config, i, CurrentDelta(i));
  }
}

void State::RecordHeadTransitions(
    size_t lfo_idx, const fridge::state::LFOTransition& transition,
    const config::Config& previous_output) {
  // Invalid LFO indexes cannot produce a meaningful transition record.
  if (lfo_idx >= NUM_LFOS) {
    return;
  }

  // Find every head position affected by this LFO. Other targets may still be
  // modulated, but only head position jumps need a motion fade record.
  for (size_t target_idx = 0; target_idx < MAX_TARGET_PARAMS; ++target_idx) {
    const std::optional<config::Target>& target =
        output_config_.lfos[lfo_idx].targets[target_idx];
    // Empty slots, non-head targets, non-position targets, and invalid head
    // indexes do not participate in head transition fading.
    if (!target.has_value() || target->object != config::TargetObject::kHead ||
        target->parameter != config::TargetParameter::kPosition ||
        target->object_idx >= NUM_HEADS) {
      continue;
    }

    size_t head_idx = target->object_idx;
    // Capture the motion before and after the transform. The transition mixer
    // uses this pair to fade from the old moving head to the new moving head.
    head_transitions_[head_idx] = fridge::transition::HeadMotionTransition{
        .old_motion =
            fridge::transition::HeadMotion{
                .position = previous_output.heads[head_idx].position,
                .direction = transition.old_direction,
                .speed = transition.old_speed,
            },
        .new_motion =
            fridge::transition::HeadMotion{
                .position = output_config_.heads[head_idx].position,
                .direction = transition.new_direction,
                .speed = transition.new_speed,
            },
        .reversed = transition.reversed,
        .teleported = transition.teleported,
    };
  }
}

void State::RebaseRootConfig(const config::Config& root_config) {
  root_config_ = SanitizeConfig(root_config);
  output_config_ = root_config_;
  ApplyCurrentDeltas(output_config_);
}

float State::CurrentDelta(size_t lfo_idx) const {
  return lfo_engines_[lfo_idx].value() - lfo_initial_values_[lfo_idx];
}

const config::Config& State::Reset(const config::Config& root_config) {
  Initialize(root_config);
  return output_config_;
}

const config::Config& State::Update(const config::Config& root_config,
                                    Samples<uint32_t> step) {
  head_transitions_ = {};

  if (!initialized_) {
    Initialize(root_config);
  } else if (!MatchesSanitizedConfig(root_config, root_config_)) {
    RebaseRootConfig(root_config);
  }

  if (step == Samples(0)) {
    return output_config_;
  }

  std::array<float, NUM_LFOS> previous_deltas{};
  for (size_t i = 0; i < NUM_LFOS; ++i) {
    previous_deltas[i] = CurrentDelta(i);
  }
  config::Config previous_output = output_config_;

  for (size_t i = 0; i < NUM_LFOS; ++i) {
    lfo_engines_[i].SetConfig(output_config_.lfos[i]);
  }

  std::array<std::optional<fridge::state::LFOTransition>, NUM_LFOS>
      lfo_transitions{};
  for (size_t i = 0; i < NUM_LFOS; ++i) {
    fridge::state::LFOTickResult tick_result =
        lfo_engines_[i].TickWithEvents(step);
    lfo_transitions[i] = tick_result.transition;
  }

  time_ += step;

  for (size_t i = 0; i < NUM_LFOS; ++i) {
    float delta_change = CurrentDelta(i) - previous_deltas[i];
    ApplyLfoDelta(output_config_, i, delta_change);
  }

  for (size_t i = 0; i < NUM_LFOS; ++i) {
    if (lfo_transitions[i].has_value()) {
      RecordHeadTransitions(i, lfo_transitions[i].value(), previous_output);
    }
  }

  return output_config_;
}

}  // namespace fridge::transform
