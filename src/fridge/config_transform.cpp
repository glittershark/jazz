#include "config_transform.hpp"

#include <algorithm>
#include <cmath>

namespace {

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

size_t ClampSize(float value, size_t minimum) {
  if (!std::isfinite(value)) {
    return minimum;
  }

  return static_cast<size_t>(
      std::max<float>(static_cast<float>(minimum), std::lround(value)));
}

fridge::config::LFO SanitizeLfo(const fridge::config::LFO& lfo) {
  fridge::config::LFO sanitized(
      ClampSize(static_cast<float>(lfo.range()), 0),
      ClampSize(static_cast<float>(lfo.max_grain_size()), 1),
      ClampSize(static_cast<float>(lfo.min_grain_size()), 1),
      ClampChance(lfo.reverse_chance()),
      ClampChance(lfo.teleport_chance()),
      ClampChance(lfo.pitch_shift_chance()),
      ClampChance(lfo.low_octave_chance()),
      ClampChance(lfo.high_octave_chance()));
  sanitized.targets() = lfo.targets();
  return sanitized;
}

fridge::config::Config SanitizeConfig(
    const fridge::config::Config& root_config) {
  fridge::config::Config sanitized = root_config;

  for (auto& head : sanitized.heads()) {
    head.write_amount() = ClampFinite(head.write_amount());
    head.read_amount() = ClampFinite(head.read_amount());
    head.erase_amount() = ClampFinite(head.erase_amount());
    head.feedback().amount() = ClampFinite(head.feedback().amount());
  }

  for (size_t i = 0; i < NUM_LFOS; ++i) {
    sanitized.lfos()[i] = SanitizeLfo(sanitized.lfos()[i]);
  }

  sanitized.dry() = ClampFinite(sanitized.dry());
  sanitized.wet() = ClampFinite(sanitized.wet());
  return sanitized;
}

void ApplyTargetDelta(fridge::config::Config& config,
                      const fridge::config::Target& target, float delta) {
  using fridge::config::TargetObject;
  using fridge::config::TargetParameter;

  if (target.object() == TargetObject::kHead) {
    if (target.object_idx() >= NUM_HEADS) {
      return;
    }

    fridge::config::Head& head = config.heads()[target.object_idx()];
    switch (target.parameter()) {
      case TargetParameter::kPosition:
        head.position() =
            ClampSize(static_cast<float>(head.position()) + delta, 0);
        return;
      case TargetParameter::kWriteAmount:
        head.write_amount() = ClampFinite(head.write_amount() + delta);
        return;
      case TargetParameter::kReadAmount:
        head.read_amount() = ClampFinite(head.read_amount() + delta);
        return;
      case TargetParameter::kEraseAmount:
        head.erase_amount() = ClampFinite(head.erase_amount() + delta);
        return;
      case TargetParameter::kFeedbackAmount:
        head.feedback().amount() =
            ClampFinite(head.feedback().amount() + delta);
        return;
      default:
        return;
    }
  }

  if (target.object() == TargetObject::kLFO) {
    if (target.object_idx() >= NUM_LFOS) {
      return;
    }

    fridge::config::LFO& lfo = config.lfos()[target.object_idx()];
    switch (target.parameter()) {
      case TargetParameter::kRange:
        lfo.range() = ClampSize(static_cast<float>(lfo.range()) + delta, 0);
        return;
      case TargetParameter::kMaxGrainSize:
        lfo.max_grain_size() =
            ClampSize(static_cast<float>(lfo.max_grain_size()) + delta, 1);
        return;
      case TargetParameter::kMinGrainSize:
        lfo.min_grain_size() =
            ClampSize(static_cast<float>(lfo.min_grain_size()) + delta, 1);
        return;
      case TargetParameter::kReverseChance:
        lfo.reverse_chance() = ClampChance(lfo.reverse_chance() + delta);
        return;
      case TargetParameter::kTeleportChance:
        lfo.teleport_chance() = ClampChance(lfo.teleport_chance() + delta);
        return;
      case TargetParameter::kPitchShiftChance:
        lfo.pitch_shift_chance() = ClampChance(lfo.pitch_shift_chance() + delta);
        return;
      case TargetParameter::kLowOctaveChance:
        lfo.low_octave_chance() = ClampChance(lfo.low_octave_chance() + delta);
        return;
      case TargetParameter::kHighOctaveChance:
        lfo.high_octave_chance() =
            ClampChance(lfo.high_octave_chance() + delta);
        return;
      default:
        return;
    }
  }

  if (target.object() != TargetObject::kMixer) {
    return;
  }

  switch (target.parameter()) {
    case TargetParameter::kDry:
      config.dry() = ClampFinite(config.dry() + delta);
      return;
    case TargetParameter::kWet:
      config.wet() = ClampFinite(config.wet() + delta);
      return;
    default:
      return;
  }
}

void ApplyLfoDelta(fridge::config::Config& config, size_t lfo_idx, float delta) {
  if (lfo_idx >= NUM_LFOS || delta == 0.0f) {
    return;
  }

  for (size_t target_idx = 0; target_idx < MAX_TARGET_PARAMS; ++target_idx) {
    if (config.lfos()[lfo_idx].targets()[target_idx].has_value()) {
      ApplyTargetDelta(config, config.lfos()[lfo_idx].targets()[target_idx].value(),
                       delta);
    }
  }
}

}  // namespace

namespace fridge {

void LFOSystem::Initialize(const config::Config& root_config) {
  time_ = 0.0f;
  initialized_ = true;
  config::Config sanitized_root = SanitizeConfig(root_config);

  for (size_t i = 0; i < NUM_LFOS; ++i) {
    config::LFO lfo_config = sanitized_root.lfos()[i];
    lfo_engines_[i] =
        state::LFOEngine(lfo_config, seed_ + static_cast<uint32_t>(i));
    lfo_engines_[i].Reset(static_cast<float>(lfo_config.range()) * 0.5f,
                          state::Direction::kForwards);
    lfo_initial_values_[i] = lfo_engines_[i].value();
  }
}

config::Config LFOSystem::Reset(const config::Config& root_config) {
  Initialize(root_config);
  return BuildVirtualConfig(root_config);
}

config::Config LFOSystem::Update(const config::Config& root_config, float dt) {
  if (!initialized_) {
    Initialize(root_config);
  }

  float step = std::max(0.0f, dt);
  config::Config output_config = BuildVirtualConfig(root_config);

  if (step <= 0.0f) {
    return output_config;
  }

  std::array<float, NUM_LFOS> previous_deltas{};
  for (size_t i = 0; i < NUM_LFOS; ++i) {
    previous_deltas[i] = lfo_engines_[i].value() - lfo_initial_values_[i];
  }

  for (size_t i = 0; i < NUM_LFOS; ++i) {
    lfo_engines_[i].SetConfig(output_config.lfos()[i]);
  }

  for (state::LFOEngine& engine : lfo_engines_) {
    engine.Tick(step);
  }

  time_ += step;

  for (size_t i = 0; i < NUM_LFOS; ++i) {
    float delta_change =
        (lfo_engines_[i].value() - lfo_initial_values_[i]) - previous_deltas[i];
    ApplyLfoDelta(output_config, i, delta_change);
  }

  return output_config;
}

config::Config LFOSystem::BuildVirtualConfig(
    const config::Config& root_config) const {
  config::Config virtual_config = SanitizeConfig(root_config);

  for (size_t i = 0; i < NUM_LFOS; ++i) {
    ApplyLfoDelta(virtual_config, i,
                  lfo_engines_[i].value() - lfo_initial_values_[i]);
  }

  return virtual_config;
}

}  // namespace fridge
