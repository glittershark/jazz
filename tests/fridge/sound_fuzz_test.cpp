#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>

#include "config.hpp"
#include "config_transform.hpp"
#include "constants.hpp"
#include "fuzztest/fuzztest.h"
#include "fuzztest/fuzztest_macros.h"
#include "libjazz/stereo_sample.hpp"
#include "libjazz/units.hpp"
#include "sound.hpp"

using fridge::BUFFER_LEN;
using fridge::MAX_TARGET_PARAMS;
using fridge::NUM_HEADS;
using fridge::NUM_LFOS;
using fridge::config::Config;
using fridge::config::Feedback;
using fridge::config::Head;
using fridge::config::LFO;
using fridge::config::Target;
using fridge::config::TargetObject;
using fridge::config::TargetParameter;
using fridge::sound::Sound;
using jazz::audio::StereoSample;
using jazz::units::Samples;

namespace fridge::config {

template <typename Sink>
void FuzzTestPrintSourceCode(Sink& sink, const Target& v) {
  absl::Format(&sink,
               "fridge::config::Target{.object = "
               "static_cast<fridge::config::TargetObject>(%d), "
               ".parameter = static_cast<fridge::config::TargetParameter>(%d), "
               ".object_idx = %zu}",
               static_cast<int>(v.object), static_cast<int>(v.parameter),
               v.object_idx);
}

template <typename Sink>
void FuzzTestPrintSourceCode(Sink& sink, const LFO& v) {
  absl::Format(&sink,
               "fridge::config::LFO{.range = %zu, .max_grain_size = %zu, "
               ".min_grain_size = %zu, .reverse_chance = %f, .teleport_chance "
               "= %f, .pitch_shift_chance = %f, .low_octave_chance = %f, "
               ".high_octave_chance = %f, .targets = {",
               v.range, v.max_grain_size, v.min_grain_size, v.reverse_chance,
               v.teleport_chance, v.pitch_shift_chance, v.low_octave_chance,
               v.high_octave_chance);
  for (size_t i = 0; i < v.targets.size(); ++i) {
    if (i > 0) {
      absl::Format(&sink, ", ");
    }
    if (v.targets[i].has_value()) {
      FuzzTestPrintSourceCode(sink, *v.targets[i]);
    } else {
      absl::Format(&sink, "std::nullopt");
    }
  }
  absl::Format(&sink, "}}");
}

auto ValidFeedback() {
  return fuzztest::StructOf<Feedback>(
      fuzztest::ElementOf({Feedback::Kind::kRead, Feedback::Kind::kErase}),
      fuzztest::Finite<float>());
}

auto ValidHead() {
  return fuzztest::StructOf<Head>(
      fuzztest::InRange<size_t>(0, BUFFER_LEN - 1), fuzztest::Finite<float>(),
      fuzztest::Finite<float>(), fuzztest::Finite<float>(), ValidFeedback(),
      fuzztest::ConstructorOf<jazz::audio::Pan>(
          fuzztest::InRange<float>(-1.0f, 1.0f)));
}

auto ValidTarget() {
  return fuzztest::StructOf<Target>(
      fuzztest::ElementOf(
          {TargetObject::kHead, TargetObject::kLFO, TargetObject::kMixer}),
      fuzztest::ElementOf({
          TargetParameter::kPosition,
          TargetParameter::kWriteAmount,
          TargetParameter::kReadAmount,
          TargetParameter::kEraseAmount,
          TargetParameter::kFeedbackAmount,
          TargetParameter::kRange,
          TargetParameter::kMaxGrainSize,
          TargetParameter::kMinGrainSize,
          TargetParameter::kReverseChance,
          TargetParameter::kTeleportChance,
          TargetParameter::kPitchShiftChance,
          TargetParameter::kLowOctaveChance,
          TargetParameter::kHighOctaveChance,
          TargetParameter::kPan,
          TargetParameter::kDry,
          TargetParameter::kWet,
      }),
      fuzztest::InRange<size_t>(0, std::min(NUM_HEADS, NUM_LFOS) - 1));
}

auto ValidLFO() {
  return fuzztest::Map(
      [](size_t range, size_t gs1, size_t gs2, float reverse_chance,
         float teleport_chance, float pitch_shift_chance,
         float low_octave_chance, float high_octave_chance,
         std::array<std::optional<Target>, MAX_TARGET_PARAMS> targets) {
        return LFO{
            .range = range,
            .max_grain_size = std::max(gs1, gs2),
            .min_grain_size = std::min(gs1, gs2),
            .reverse_chance = reverse_chance,
            .teleport_chance = teleport_chance,
            .pitch_shift_chance = pitch_shift_chance,
            .low_octave_chance = low_octave_chance,
            .high_octave_chance = high_octave_chance,
            .targets = targets,
        };
      },
      fuzztest::InRange<size_t>(0, BUFFER_LEN),
      fuzztest::InRange<size_t>(1, BUFFER_LEN),
      fuzztest::InRange<size_t>(1, BUFFER_LEN),
      fuzztest::InRange<float>(0.0f, 1.0f),
      fuzztest::InRange<float>(0.0f, 1.0f),
      fuzztest::InRange<float>(0.0f, 1.0f),
      fuzztest::InRange<float>(0.0f, 1.0f),
      fuzztest::InRange<float>(0.0f, 1.0f),
      fuzztest::ArrayOf<MAX_TARGET_PARAMS>(
          fuzztest::OptionalOf(ValidTarget())));
}

auto ValidConfig() {
  return fuzztest::StructOf<Config>(fuzztest::ArrayOf<NUM_HEADS>(ValidHead()),
                                    fuzztest::ArrayOf<NUM_LFOS>(ValidLFO()),
                                    fuzztest::InRange<float>(0.0f, 1.0f),
                                    fuzztest::InRange<float>(0.0f, 1.0f));
}

}  // namespace fridge::config

void ProcessSampleDoesNotCrash(float sample, size_t position,
                               float write_amount, float read_amount,
                               float erase_amount) {
  Config config;
  config.heads[0] = Head{
      .position = position,
      .write_amount = write_amount,
      .read_amount = read_amount,
      .erase_amount = erase_amount,
  };

  auto sound = std::make_unique<Sound>();
  sound->ProcessSample(config, StereoSample::OfMono(sample));
}

FUZZ_TEST(FridgeSoundFuzzTest, ProcessSampleDoesNotCrash)
    .WithDomains(fuzztest::InRange<float>(-1.0f, 1.0f),
                 fuzztest::InRange<size_t>(0, BUFFER_LEN - 1),
                 fuzztest::InRange<float>(0.0f, 1.0f),
                 fuzztest::InRange<float>(0.0f, 1.0f),
                 fuzztest::InRange<float>(0.0f, 1.0f));

void StaticConfigNeverCrashes(Config root_config,
                              const std::vector<float>& samples,
                              uint32_t seed) {
  auto transform = std::make_unique<fridge::transform::State>(seed);
  auto sound = std::make_unique<Sound>();
  for (auto&& sample : samples) {
    auto frame = transform->Update(root_config, Samples(1));
    auto res = sound->ProcessSample(frame, StereoSample::OfMono(sample));
  }
}

FUZZ_TEST(FridgeSoundFuzzTest, StaticConfigNeverCrashes)
    .WithDomains(fridge::config::ValidConfig(),
                 fuzztest::VectorOf(fuzztest::InRange<float>(-1.0f, 1.0f)),
                 fuzztest::Arbitrary<uint32_t>());

struct Event {
  float sample;
  std::optional<Config> new_config;
};

void DynamicConfigNeverCrashes(Config initial_config,
                               const std::vector<Event>& events,
                               uint32_t seed) {
  auto transform = std::make_unique<fridge::transform::State>(seed);
  auto sound = std::make_unique<Sound>();
  Config config = initial_config;
  for (auto&& event : events) {
    if (event.new_config.has_value()) {
      config = *event.new_config;
    }
    auto frame = transform->Update(config, Samples(1));
    auto res = sound->ProcessSample(frame, StereoSample::OfMono(event.sample));
  }
}

FUZZ_TEST(FridgeSoundFuzzTest, DynamicConfigNeverCrashes)
    .WithDomains(fridge::config::ValidConfig(),
                 fuzztest::VectorOf(fuzztest::StructOf<Event>(
                     fuzztest::InRange<float>(-1.0f, 1.0f),
                     fuzztest::OptionalOf(fridge::config::ValidConfig()))),
                 fuzztest::Arbitrary<uint32_t>());
