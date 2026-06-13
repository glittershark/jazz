#include <algorithm>
#include <cstdint>
#include <memory>

#include "config.hpp"
#include "config_transform.hpp"
#include "constants.hpp"
#include "fuzztest/fuzztest.h"
#include "fuzztest/fuzztest_macros.h"
#include "gtest/gtest.h"
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
      fuzztest::Finite<float>(), fuzztest::Finite<float>(), ValidFeedback());
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
  sound->ProcessSample(config, sample);
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
    auto res = sound->ProcessSample(frame, sample);
  }
}

FUZZ_TEST(FridgeSoundFuzzTest, StaticConfigNeverCrashes)
    .WithDomains(fridge::config::ValidConfig(),
                 fuzztest::VectorOf(fuzztest::InRange<float>(-1.0f, 1.0f)),
                 fuzztest::Arbitrary<uint32_t>());

TEST(FridgeSoundFuzzTest, StaticConfigNeverCrashesRegression) {
  StaticConfigNeverCrashes(
      fridge::config::Config{
          .heads =
              {fridge::config::Head{
                   .position = 1,
                   .write_amount = 0.f,
                   .read_amount = 0.0093690902f,
                   .erase_amount = -0.f,
                   .feedback =
                       fridge::config::Feedback{
                           .kind =
                               static_cast<fridge::config::Feedback::Kind>(0),
                           .amount = -0.f}},
               fridge::config::Head{
                   .position = 15361857,
                   .write_amount = -0.f,
                   .read_amount = 0.f,
                   .erase_amount = 0.422283083f,
                   .feedback =
                       fridge::config::Feedback{
                           .kind =
                               static_cast<fridge::config::Feedback::Kind>(1),
                           .amount = -1.f}}},
          .lfos =
              {fridge::config::LFO{
                   .range = 0,
                   .max_grain_size = 4276026,
                   .min_grain_size = 3318747,
                   .reverse_chance = 0.279652,
                   .teleport_chance =
                       0.412459,
                   .pitch_shift_chance = 0.000000,
                   .low_octave_chance = 0.000000,
                   .high_octave_chance = 0.000000,
                   .targets =
                       {std::nullopt,
                        fridge::config::Target{
                            .object =
                                static_cast<fridge::config::TargetObject>(0),
                            .parameter =
                                static_cast<fridge::config::TargetParameter>(1),
                            .object_idx = 1},
                        fridge::config::Target{
                            .object =
                                static_cast<fridge::config::TargetObject>(0),
                            .parameter =
                                static_cast<fridge::config::TargetParameter>(3),
                            .object_idx = 0},
                        std::nullopt,
                        fridge::config::Target{
                            .object =
                                static_cast<fridge::config::TargetObject>(2),
                            .parameter =
                                static_cast<fridge::config::TargetParameter>(2),
                            .object_idx = 0},
                        fridge::config::Target{
                            .object =
                                static_cast<fridge::config::TargetObject>(2),
                            .parameter =
                                static_cast<fridge::config::TargetParameter>(2),
                            .object_idx = 1},
                        fridge::config::Target{
                            .object =
                                static_cast<fridge::config::TargetObject>(0),
                            .parameter =
                                static_cast<fridge::config::TargetParameter>(
                                    14),
                            .object_idx = 0},
                        fridge::config::Target{
                            .object =
                                static_cast<fridge::config::TargetObject>(1),
                            .parameter =
                                static_cast<fridge::config::TargetParameter>(
                                    11),
                            .object_idx = 1}}},
               fridge::config::LFO{
                   .range = 15335319,
                   .max_grain_size = 1566339,
                   .min_grain_size = 1,
                   .reverse_chance = 0.000000,
                   .teleport_chance =
                       1.000000,
                   .pitch_shift_chance = 0.000000,
                   .low_octave_chance = 0.000000,
                   .high_octave_chance = 0.000000,
                   .targets =
                       {fridge::config::Target{
                            .object =
                                static_cast<fridge::config::TargetObject>(0),
                            .parameter =
                                static_cast<fridge::config::TargetParameter>(9),
                            .object_idx = 1},
                        fridge::config::Target{
                            .object =
                                static_cast<fridge::config::TargetObject>(2),
                            .parameter =
                                static_cast<fridge::config::TargetParameter>(5),
                            .object_idx = 1},
                        std::nullopt,
                        fridge::config::Target{
                            .object =
                                static_cast<fridge::config::TargetObject>(2),
                            .parameter =
                                static_cast<fridge::config::TargetParameter>(
                                    14),
                            .object_idx = 0},
                        fridge::config::Target{
                            .object =
                                static_cast<fridge::config::TargetObject>(1),
                            .parameter =
                                static_cast<fridge::config::TargetParameter>(
                                    14),
                            .object_idx = 0},
                        std::nullopt,
                        fridge::config::Target{
                            .object =
                                static_cast<fridge::config::TargetObject>(1),
                            .parameter =
                                static_cast<fridge::config::TargetParameter>(
                                    14),
                            .object_idx = 0},
                        std::nullopt}}},
          .dry = 1.f,
          .wet = -1.f},
      {0.769551277f}, 1551742920);
}
