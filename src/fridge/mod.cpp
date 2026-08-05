#include "mod.hpp"

#include <algorithm>
#include <bit>
#include <cmath>

namespace fridge::mod {

namespace {

float DirectionMultiplier(Direction direction) {
  return direction == Direction::kForwards ? 1.0f : -1.0f;
}

Direction ReverseDirection(Direction direction) {
  return direction == Direction::kForwards ? Direction::kBackwards
                                           : Direction::kForwards;
}

/* When several grain boundaries land inside one tick, report a single
 * transition spanning from the motion before the first to the motion after
 * the last. */
LFOTransition ExtendTransition(const LFOTransition& accumulated,
                               const LFOTransition& latest) {
  LFOTransition transition = latest;
  transition.old_value = accumulated.old_value;
  transition.old_direction = accumulated.old_direction;
  transition.old_speed = accumulated.old_speed;
  transition.reversed = accumulated.reversed || latest.reversed;
  transition.teleported = accumulated.teleported || latest.teleported;
  return transition;
}

void MergeTransition(LFOTickResult& result,
                     std::optional<LFOTransition> transition) {
  if (!transition.has_value()) {
    return;
  }
  if (result.transition.has_value()) {
    result.transition = ExtendTransition(*result.transition, *transition);
  } else {
    result.transition = transition;
  }
}

/* Wraps a fading head's extrapolated position back into the sample buffer. */
size_t WrapBufferPosition(float position) {
  float wrapped = std::fmod(position, static_cast<float>(kBufferLen));
  if (wrapped < 0.0f) {
    wrapped += static_cast<float>(kBufferLen);
  }
  return static_cast<size_t>(std::lround(wrapped)) % kBufferLen;
}

}  // namespace

// ----- Rng

Rng::Rng(uint32_t seed) {
  // splitmix-style scramble so nearby seeds diverge; xorshift state must be
  // nonzero
  uint32_t z = seed + 0x9E3779B9u;
  z = (z ^ (z >> 16)) * 0x85EBCA6Bu;
  z = (z ^ (z >> 13)) * 0xC2B2AE35u;
  z ^= z >> 16;
  state_ = z != 0 ? z : 0xA341316Cu;
}

uint32_t Rng::Next() {
  uint32_t x = state_;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return state_ = x;
}

float Rng::NextFloat() {
  return static_cast<float>(Next() >> 8) * 0x1.0p-24f;
}

bool Rng::Chance(float chance) {
  if (chance <= 0.0f) {
    return false;
  }
  if (chance >= 1.0f) {
    return true;
  }
  return NextFloat() < chance;
}

uint32_t Rng::Between(uint32_t lo, uint32_t hi) {
  return hi > lo ? lo + Next() % (hi - lo + 1) : lo;
}

// ----- LFOEngine

LFOEngine::LFOEngine(const config::LFO& config) : LFOEngine(config, 1) {}

LFOEngine::LFOEngine(const config::LFO& config, uint32_t seed) : rng_(seed) {
  SetConfig(config);
  Reset();
}

void LFOEngine::SetParams(const LfoParams& params) {
  params_ = params;
  value_ = Wrap(value_);
}

void LFOEngine::SetConfig(const config::LFO& config) {
  SetParams(LfoParams{
      .range = config.range,
      .max_grain_size = config.max_grain_size,
      .min_grain_size = config.min_grain_size,
      .reverse_chance = config.reverse_chance,
      .teleport_chance = config.teleport_chance,
      .pitch_shift_chance = config.pitch_shift_chance,
      .low_octave_chance = config.low_octave_chance,
      .high_octave_chance = config.high_octave_chance,
  });
}

config::LFO LFOEngine::config() const {
  return config::LFO{
      .range = params_.range,
      .max_grain_size = params_.max_grain_size,
      .min_grain_size = params_.min_grain_size,
      .reverse_chance = params_.reverse_chance,
      .teleport_chance = params_.teleport_chance,
      .pitch_shift_chance = params_.pitch_shift_chance,
      .low_octave_chance = params_.low_octave_chance,
      .high_octave_chance = params_.high_octave_chance,
  };
}

void LFOEngine::Reset(float initial_value, Direction direction) {
  value_ = Wrap(initial_value);
  direction_ = direction;
  StartNewGrain(true);
}

float LFOEngine::Wrap(float value) const {
  if (params_.range == 0) {
    return 0.0f;
  }
  const float range = static_cast<float>(params_.range);
  if (value >= range) {
    value -= range;
    if (value >= range) {
      value = std::fmod(value, range);
    }
  } else if (value < 0.0f) {
    value += range;
    if (value < 0.0f) {
      value = std::fmod(value, range);
      if (value < 0.0f) {
        value += range;
      }
    }
  }
  return value;
}

float LFOEngine::Tick(Samples<uint32_t> step) {
  return TickWithEvents(step).value;
}

LFOTickResult LFOEngine::TickWithEvents(Samples<uint32_t> step) {
  LFOTickResult result{.value = value_};
  uint32_t remaining = step.samples();

  while (remaining > 0) {
    if (grain_remaining_ == 0) {
      MergeTransition(result, StartNewGrain(false));
    }

    const uint32_t hop = std::min(remaining, grain_remaining_);
    value_ = Wrap(value_ + speed_ * DirectionMultiplier(direction_) *
                               static_cast<float>(hop));
    grain_remaining_ -= hop;
    remaining -= hop;

    if (grain_remaining_ == 0) {
      MergeTransition(result, StartNewGrain(false));
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
    if (rng_.Chance(params_.reverse_chance)) {
      direction_ = ReverseDirection(direction_);
      transition.reversed = true;
    }

    if (rng_.Chance(params_.teleport_chance)) {
      value_ = rng_.NextFloat() * static_cast<float>(params_.range);
      transition.teleported = true;
    }
  }

  grain_size_ = SampleGrainSize();
  speed_ = SampleSpeed();
  grain_remaining_ = grain_size_;

  if (!transition.reversed && !transition.teleported) {
    return std::nullopt;
  }

  transition.new_value = value_;
  transition.new_direction = direction_;
  transition.new_speed = speed_;
  return transition;
}

uint32_t LFOEngine::SampleGrainSize() {
  const uint32_t lo = static_cast<uint32_t>(std::max<size_t>(
      1, std::min(params_.min_grain_size, params_.max_grain_size)));
  const uint32_t hi = static_cast<uint32_t>(
      std::max<size_t>({lo, params_.min_grain_size, params_.max_grain_size}));
  return rng_.Between(lo, hi);
}

float LFOEngine::SampleSpeed() {
  if (!rng_.Chance(params_.pitch_shift_chance)) {
    return 1.0f;
  }

  const float low = std::max(0.0f, params_.low_octave_chance);
  const float high = std::max(0.0f, params_.high_octave_chance);
  const float total = low + high;

  if (total <= 0.0f) {
    return 1.0f;
  }

  return rng_.Chance(high / total) ? 2.0f : 0.5f;
}

// ----- Modulator

Modulator::Modulator(uint32_t seed, size_t fade_time)
    : seed_(seed), fade_time_(std::max<size_t>(1, fade_time)) {}

// ----- Modulator: validation

float Modulator::ClampFinite(float value, float fallback) {
  return std::isfinite(value) ? value : fallback;
}

float Modulator::ClampChance(float value) {
  if (!std::isfinite(value)) {
    return 0.0f;
  }
  return std::clamp(value, 0.0f, 1.0f);
}

size_t Modulator::ClampSize(float value, size_t minimum) {
  if (!std::isfinite(value)) {
    return minimum;
  }
  return static_cast<size_t>(
      std::max<float>(static_cast<float>(minimum), std::lround(value)));
}

config::Config Modulator::SanitizeConfig(const config::Config& root_config) {
  config::Config sanitized = root_config;

  for (config::Head& head : sanitized.heads) {
    head.write_amount = ClampFinite(head.write_amount);
    head.read_amount = ClampFinite(head.read_amount);
    head.erase_amount = ClampFinite(head.erase_amount);
    head.feedback.amount = ClampFinite(head.feedback.amount);
  }

  for (config::LFO& lfo : sanitized.lfos) {
    lfo.range = ClampSize(static_cast<float>(lfo.range), 0);
    lfo.max_grain_size = ClampSize(static_cast<float>(lfo.max_grain_size), 1);
    lfo.min_grain_size = ClampSize(static_cast<float>(lfo.min_grain_size), 1);
    lfo.reverse_chance = ClampChance(lfo.reverse_chance);
    lfo.teleport_chance = ClampChance(lfo.teleport_chance);
    lfo.pitch_shift_chance = ClampChance(lfo.pitch_shift_chance);
    lfo.low_octave_chance = ClampChance(lfo.low_octave_chance);
    lfo.high_octave_chance = ClampChance(lfo.high_octave_chance);
  }

  sanitized.dry = ClampFinite(sanitized.dry);
  sanitized.wet = ClampFinite(sanitized.wet);
  return sanitized;
}

// ----- Modulator: control plane

std::optional<size_t> Modulator::ParamSlot(const config::Target& target) {
  using config::TargetObject;
  using config::TargetParameter;

  switch (target.object) {
  case TargetObject::kHead: {
    if (target.object_idx >= kNumHeads) {
      return std::nullopt;
    }
    const size_t base = target.object_idx * kHeadParamCount;
    switch (target.parameter) {
    case TargetParameter::kPosition:
      return base + 0;
    case TargetParameter::kWriteAmount:
      return base + 1;
    case TargetParameter::kReadAmount:
      return base + 2;
    case TargetParameter::kEraseAmount:
      return base + 3;
    case TargetParameter::kFeedbackAmount:
      return base + 4;
    default:
      return std::nullopt;
    }
  }
  case TargetObject::kLFO: {
    if (target.object_idx >= kNumLfos) {
      return std::nullopt;
    }
    const size_t base = kLfoParamBase + target.object_idx * kLfoParamCount;
    switch (target.parameter) {
    case TargetParameter::kRange:
      return base + 0;
    case TargetParameter::kMaxGrainSize:
      return base + 1;
    case TargetParameter::kMinGrainSize:
      return base + 2;
    case TargetParameter::kReverseChance:
      return base + 3;
    case TargetParameter::kTeleportChance:
      return base + 4;
    case TargetParameter::kPitchShiftChance:
      return base + 5;
    case TargetParameter::kLowOctaveChance:
      return base + 6;
    case TargetParameter::kHighOctaveChance:
      return base + 7;
    default:
      return std::nullopt;
    }
  }
  case TargetObject::kMixer:
    switch (target.parameter) {
    case TargetParameter::kDry:
      return kMixerParamBase + 0;
    case TargetParameter::kWet:
      return kMixerParamBase + 1;
    default:
      return std::nullopt;
    }
  }

  return std::nullopt;
}

void Modulator::CompilePatches() {
  patch_count_ = 0;
  modulated_heads_ = 0;
  modulated_lfos_ = 0;
  mixer_modulated_ = false;

  for (size_t lfo_idx = 0; lfo_idx < kNumLfos; ++lfo_idx) {
    for (const std::optional<config::Target>& target :
         base_.lfos[lfo_idx].targets) {
      if (!target.has_value()) {
        continue;
      }
      const std::optional<size_t> slot = ParamSlot(*target);
      if (!slot.has_value() || patch_count_ >= patches_.size()) {
        continue;
      }

      patches_[patch_count_++] = Patch{
          .slot = static_cast<uint16_t>(*slot),
          .lfo = static_cast<uint8_t>(lfo_idx),
      };

      if (*slot < kLfoParamBase) {
        modulated_heads_ |= 1u << (*slot / kHeadParamCount);
      } else if (*slot < kMixerParamBase) {
        modulated_lfos_ |= 1u << ((*slot - kLfoParamBase) / kLfoParamCount);
      } else {
        mixer_modulated_ = true;
      }
    }
  }
}

void Modulator::Initialize(const config::Config& root_config) {
  initialized_ = true;
  time_ = Samples(0u);
  base_ = SanitizeConfig(root_config);
  CompilePatches();
  mod_ = {};
  fades_ = {};

  for (size_t i = 0; i < kNumLfos; ++i) {
    engines_[i] = LFOEngine(base_.lfos[i], seed_ + static_cast<uint32_t>(i));
    engines_[i].Reset(0.0f, Direction::kForwards);
    anchors_[i] = engines_[i].value();
  }

  eff_heads_ = base_.heads;
  BuildFrame();
  output_dirty_ = true;
}

void Modulator::SetConfig(const config::Config& root_config) {
  if (!initialized_) {
    Initialize(root_config);
    return;
  }

  base_ = SanitizeConfig(root_config);
  CompilePatches();
  mod_ = {};
  RecomputeMods();

  for (size_t i = 0; i < kNumLfos; ++i) {
    engines_[i].SetParams(EffectiveLfoParams(i));
  }

  eff_heads_ = base_.heads;
  for (size_t h = 0; h < kNumHeads; ++h) {
    if ((modulated_heads_ & (1u << h)) != 0) {
      UpdateEffectiveHead(h);
    }
  }

  BuildFrame();
  output_dirty_ = true;
}

const config::Config& Modulator::Reset(const config::Config& root_config) {
  Initialize(root_config);
  return virtual_config();
}

// ----- Modulator: audio plane

void Modulator::RecomputeMods() {
  for (size_t p = 0; p < patch_count_; ++p) {
    mod_[patches_[p].slot] = 0.0f;
  }
  for (size_t p = 0; p < patch_count_; ++p) {
    const Patch& patch = patches_[p];
    mod_[patch.slot] += engines_[patch.lfo].value() - anchors_[patch.lfo];
  }
}

LfoParams Modulator::EffectiveLfoParams(size_t lfo_idx) const {
  const config::LFO& base = base_.lfos[lfo_idx];
  LfoParams params{
      .range = base.range,
      .max_grain_size = base.max_grain_size,
      .min_grain_size = base.min_grain_size,
      .reverse_chance = base.reverse_chance,
      .teleport_chance = base.teleport_chance,
      .pitch_shift_chance = base.pitch_shift_chance,
      .low_octave_chance = base.low_octave_chance,
      .high_octave_chance = base.high_octave_chance,
  };

  if ((modulated_lfos_ & (1u << lfo_idx)) == 0) {
    return params;
  }

  const float* mod = &mod_[kLfoParamBase + lfo_idx * kLfoParamCount];
  params.range = ClampSize(static_cast<float>(base.range) + mod[0], 0);
  params.max_grain_size =
      ClampSize(static_cast<float>(base.max_grain_size) + mod[1], 1);
  params.min_grain_size =
      ClampSize(static_cast<float>(base.min_grain_size) + mod[2], 1);
  params.reverse_chance = ClampChance(base.reverse_chance + mod[3]);
  params.teleport_chance = ClampChance(base.teleport_chance + mod[4]);
  params.pitch_shift_chance = ClampChance(base.pitch_shift_chance + mod[5]);
  params.low_octave_chance = ClampChance(base.low_octave_chance + mod[6]);
  params.high_octave_chance = ClampChance(base.high_octave_chance + mod[7]);
  return params;
}

void Modulator::UpdateEffectiveHead(size_t head_idx) {
  const config::Head& base = base_.heads[head_idx];
  const float* mod = &mod_[head_idx * kHeadParamCount];
  config::Head& head = eff_heads_[head_idx];

  head.position = ClampSize(static_cast<float>(base.position) + mod[0], 0);
  head.write_amount = ClampFinite(base.write_amount + mod[1]);
  head.read_amount = ClampFinite(base.read_amount + mod[2]);
  head.erase_amount = ClampFinite(base.erase_amount + mod[3]);
  head.feedback.kind = base.feedback.kind;
  head.feedback.amount = ClampFinite(base.feedback.amount + mod[4]);
}

void Modulator::BeginFades(size_t lfo_idx, const LFOTransition& transition) {
  if (!transition.reversed && !transition.teleported) {
    return;
  }

  for (const std::optional<config::Target>& target :
       base_.lfos[lfo_idx].targets) {
    if (!target.has_value() || target->object != config::TargetObject::kHead ||
        target->parameter != config::TargetParameter::kPosition ||
        target->object_idx >= kNumHeads) {
      continue;
    }

    // eff_heads_ still holds last sample's values here, so the fade departs
    // from where the head actually was before the jump.
    const size_t head_idx = target->object_idx;
    fades_[head_idx] = Fade{
        .old_head = eff_heads_[head_idx],
        .old_position = static_cast<float>(eff_heads_[head_idx].position),
        .old_velocity = transition.old_speed *
                        DirectionMultiplier(transition.old_direction),
        .remaining = static_cast<uint32_t>(fade_time_),
    };
  }
}

float Modulator::effective_dry() const {
  return mixer_modulated_ ? ClampFinite(base_.dry + mod_[kMixerParamBase + 0])
                          : base_.dry;
}

float Modulator::effective_wet() const {
  return mixer_modulated_ ? ClampFinite(base_.wet + mod_[kMixerParamBase + 1])
                          : base_.wet;
}

void Modulator::AddHead(config::Head head, float weight) {
  if (frame_.head_count >= frame_.heads.size() || weight <= 0.0f) {
    return;
  }

  const float clamped = std::clamp(weight, 0.0f, 1.0f);
  head.write_amount *= clamped;
  head.read_amount *= clamped;
  head.erase_amount = 1.0f - ((1.0f - head.erase_amount) * clamped);
  head.feedback.amount *= clamped;
  frame_.heads[frame_.head_count++] = head;
}

void Modulator::BuildFrame() {
  frame_.head_count = 0;
  frame_.dry = effective_dry();
  frame_.wet = effective_wet();

  for (size_t h = 0; h < kNumHeads; ++h) {
    const Fade& fade = fades_[h];
    if (fade.remaining == 0) {
      AddHead(eff_heads_[h], 1.0f);
      continue;
    }

    const float old_weight =
        static_cast<float>(fade.remaining) / static_cast<float>(fade_time_);
    config::Head old_head = fade.old_head;
    old_head.position = WrapBufferPosition(fade.old_position);
    AddHead(old_head, old_weight);
    AddHead(eff_heads_[h], 1.0f - old_weight);
  }
}

void Modulator::AdvanceFades() {
  for (Fade& fade : fades_) {
    if (fade.remaining == 0) {
      continue;
    }
    fade.old_position += fade.old_velocity;
    fade.remaining--;
  }
}

const Frame& Modulator::TickSample() {
  // LFO-on-LFO modulation: refresh params for engines someone is modulating,
  // using last sample's deltas.
  uint32_t refresh = modulated_lfos_;
  while (refresh != 0) {
    const unsigned i = std::countr_zero(refresh);
    refresh &= refresh - 1;
    engines_[i].SetParams(EffectiveLfoParams(i));
  }

  std::array<std::optional<LFOTransition>, kNumLfos> transitions{};
  for (size_t i = 0; i < kNumLfos; ++i) {
    transitions[i] = engines_[i].TickWithEvents(Samples(1u)).transition;
  }

  RecomputeMods();

  // Fades capture eff_heads_ before this sample's positions land.
  for (size_t i = 0; i < kNumLfos; ++i) {
    if (transitions[i].has_value()) {
      BeginFades(i, *transitions[i]);
    }
  }

  uint32_t heads = modulated_heads_;
  while (heads != 0) {
    const unsigned h = std::countr_zero(heads);
    heads &= heads - 1;
    UpdateEffectiveHead(h);
  }

  BuildFrame();
  AdvanceFades();

  time_ += Samples(1u);
  output_dirty_ = true;
  return frame_;
}

// ----- Modulator: host / test entry points

const config::Config& Modulator::Update(const config::Config& root_config,
                                        Samples<uint32_t> step) {
  if (!initialized_) {
    Initialize(root_config);
  } else if (!(SanitizeConfig(root_config) == base_)) {
    SetConfig(root_config);
  }

  for (uint32_t i = 0; i < step.samples(); ++i) {
    TickSample();
  }

  return virtual_config();
}

const config::Config& Modulator::virtual_config() {
  if (!output_dirty_) {
    return output_config_;
  }

  output_config_ = base_;
  output_config_.heads = eff_heads_;

  uint32_t lfos = modulated_lfos_;
  while (lfos != 0) {
    const unsigned i = std::countr_zero(lfos);
    lfos &= lfos - 1;
    const LfoParams params = EffectiveLfoParams(i);
    config::LFO& lfo = output_config_.lfos[i];
    lfo.range = params.range;
    lfo.max_grain_size = params.max_grain_size;
    lfo.min_grain_size = params.min_grain_size;
    lfo.reverse_chance = params.reverse_chance;
    lfo.teleport_chance = params.teleport_chance;
    lfo.pitch_shift_chance = params.pitch_shift_chance;
    lfo.low_octave_chance = params.low_octave_chance;
    lfo.high_octave_chance = params.high_octave_chance;
  }

  output_config_.dry = effective_dry();
  output_config_.wet = effective_wet();
  output_dirty_ = false;
  return output_config_;
}

}  // namespace fridge::mod
