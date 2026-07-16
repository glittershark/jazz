#include <limits>

#include "config.hpp"
#include "fridge.hpp"
#include "gtest/gtest.h"
#include "libjazz/units.hpp"

using fridge::config::LFO;
using fridge::config::Target;
using fridge::config::TargetObject;
using fridge::config::TargetParameter;
using fridge::transform::State;
using jazz::units::Samples;

namespace {

LFO DeterministicLfo(size_t range = 10, size_t grain_size = 4) {
  return LFO{.range = range,
             .max_grain_size = grain_size,
             .min_grain_size = grain_size};
}

}  // namespace

TEST(FridgeLFOSystemTest, ResetStartsAtStaticKnobPositions) {
  fridge::config::Config config;
  config.heads[0].position = 12;
  config.heads[0].write_amount = 0.25f;
  config.heads[0].feedback.amount = 0.75f;
  config.lfos[0].range = 10;
  config.lfos[0].max_grain_size = 4;
  config.lfos[0].min_grain_size = 2;
  config.dry = 0.3f;
  config.wet = 0.9f;

  State system(1234);
  fridge::config::Config output = system.Reset(config);

  EXPECT_EQ(output.heads[0].position, 12u);
  EXPECT_FLOAT_EQ(output.heads[0].write_amount, 0.25f);
  EXPECT_FLOAT_EQ(output.heads[0].feedback.amount, 0.75f);
  EXPECT_EQ(output.lfos[0].range, 10u);
  EXPECT_EQ(output.lfos[0].max_grain_size, 4u);
  EXPECT_FLOAT_EQ(output.dry, 0.3f);
  EXPECT_FLOAT_EQ(output.wet, 0.9f);
  EXPECT_EQ(system.time(), Samples(0));
}

TEST(FridgeLFOSystemTest, ResetAndUpdateReturnStableConfigReference) {
  fridge::config::Config config;
  config.lfos[0] = DeterministicLfo();
  config.lfos[0].targets[0] = Target{.object = TargetObject::kMixer,
                                     .parameter = TargetParameter::kDry};

  State system(1234);
  const fridge::config::Config* reset_output = &system.Reset(config);
  const fridge::config::Config* paused_output =
      &system.Update(config, Samples(0));

  EXPECT_EQ(reset_output, paused_output);
  EXPECT_FLOAT_EQ(paused_output->dry, 1.0f);
}

TEST(FridgeLFOSystemTest, UpdateCreatesVirtualHeadKnobPositions) {
  fridge::config::Config config;
  config.heads[0].write_amount = 0.25f;
  config.lfos[0] = DeterministicLfo();
  config.lfos[0].targets[0] = Target{.object = TargetObject::kHead,
                                     .parameter = TargetParameter::kWriteAmount,
                                     .object_idx = 0};

  State system(1234);
  fridge::config::Config initial = system.Reset(config);

  EXPECT_FLOAT_EQ(initial.heads[0].write_amount, 0.25f);

  fridge::config::Config output = system.Update(config, Samples(2));

  EXPECT_FLOAT_EQ(config.heads[0].write_amount, 0.25f);
  EXPECT_FLOAT_EQ(output.heads[0].write_amount, 2.25f);
  EXPECT_EQ(system.time(), Samples(2));
}

TEST(FridgeLFOSystemTest, LfoCanModulateMixerKnobs) {
  fridge::config::Config config;
  config.dry = 0.2f;
  config.wet = 0.4f;
  config.lfos[0] = DeterministicLfo();
  config.lfos[0].targets[0] = Target{.object = TargetObject::kMixer,
                                     .parameter = TargetParameter::kDry};
  config.lfos[0].targets[1] = Target{.object = TargetObject::kMixer,
                                     .parameter = TargetParameter::kWet};

  State system(1234);
  system.Reset(config);
  fridge::config::Config output = system.Update(config, Samples(1));

  EXPECT_FLOAT_EQ(output.dry, 1.2f);
  EXPECT_FLOAT_EQ(output.wet, 1.4f);
}

TEST(FridgeLFOSystemTest, LfoModulatesAnotherLfoOnTheNextTick) {
  fridge::config::Config config;
  config.dry = 0.0f;
  config.lfos[0] = DeterministicLfo(10, 4);
  config.lfos[0].targets[0] =
      Target{.object = TargetObject::kLFO,
             .parameter = TargetParameter::kReverseChance,
             .object_idx = 1};
  config.lfos[1] = DeterministicLfo(10, 1);
  config.lfos[1].targets[0] = Target{.object = TargetObject::kMixer,
                                     .parameter = TargetParameter::kDry};

  State system(1234);
  fridge::config::Config initial = system.Reset(config);

  EXPECT_FLOAT_EQ(initial.lfos[1].reverse_chance, 0.0f);

  fridge::config::Config first_output = system.Update(config, Samples(1));

  EXPECT_FLOAT_EQ(first_output.lfos[1].reverse_chance, 1.0f);
  EXPECT_FLOAT_EQ(first_output.dry, 1.0f);

  fridge::config::Config second_output = system.Update(config, Samples(1));
  fridge::config::Config third_output = system.Update(config, Samples(1));

  EXPECT_FLOAT_EQ(second_output.dry, 2.0f);
  EXPECT_FLOAT_EQ(third_output.dry, 1.0f);
}

TEST(FridgeLFOSystemTest, ResetRestoresStaticVirtualState) {
  fridge::config::Config config;
  config.heads[0].position = 9;
  config.lfos[0] = DeterministicLfo();
  config.lfos[0].targets[0] = Target{.object = TargetObject::kHead,
                                     .parameter = TargetParameter::kPosition,
                                     .object_idx = 0};

  State system(1234);
  system.Reset(config);
  fridge::config::Config modulated = system.Update(config, Samples(2));
  ASSERT_NE(modulated.heads[0].position, 9u);

  fridge::config::Config reset = system.Reset(config);

  EXPECT_EQ(reset.heads[0].position, 9u);
  EXPECT_EQ(system.time(), Samples(0));
}

TEST(FridgeLFOSystemTest, InvalidTargetsAreIgnored) {
  fridge::config::Config config;
  config.heads[0].write_amount = 0.25f;
  config.lfos[0] = DeterministicLfo();
  config.lfos[0].targets[0] = Target{.object = TargetObject::kHead,
                                     .parameter = TargetParameter::kWriteAmount,
                                     .object_idx = 99};
  config.lfos[0].targets[1] =
      Target{.object = TargetObject::kMixer,
             .parameter = TargetParameter::kWriteAmount};

  State system(1234);
  system.Reset(config);
  fridge::config::Config output = system.Update(config, Samples(2));

  EXPECT_FLOAT_EQ(output.heads[0].write_amount, 0.25f);
}

TEST(FridgeLFOSystemTest, ModulatedLfoRangeAffectsNextTick) {
  fridge::config::Config config;
  config.dry = 0.0f;
  config.lfos[0] = DeterministicLfo(10, 4);
  config.lfos[0].targets[0] = Target{.object = TargetObject::kLFO,
                                     .parameter = TargetParameter::kRange,
                                     .object_idx = 1};
  config.lfos[1] = DeterministicLfo(0, 1);
  config.lfos[1].targets[0] = Target{.object = TargetObject::kMixer,
                                     .parameter = TargetParameter::kDry};

  State system(1234);
  fridge::config::Config initial = system.Reset(config);
  EXPECT_EQ(initial.lfos[1].range, 0u);

  fridge::config::Config first_output = system.Update(config, Samples(2));
  EXPECT_EQ(first_output.lfos[1].range, 2u);

  fridge::config::Config second_output = system.Update(config, Samples(1));
  EXPECT_FLOAT_EQ(second_output.dry, 1.0f);
}

TEST(FridgeLFOSystemTest, NonPositiveDtReturnsCurrentVirtualConfig) {
  fridge::config::Config config;
  config.lfos[0] = DeterministicLfo();
  config.lfos[0].targets[0] = Target{.object = TargetObject::kMixer,
                                     .parameter = TargetParameter::kDry};

  State system(1234);
  system.Reset(config);
  fridge::config::Config advanced = system.Update(config, Samples(1));
  fridge::config::Config paused = system.Update(config, Samples(0));

  EXPECT_FLOAT_EQ(paused.dry, advanced.dry);
  EXPECT_EQ(system.time(), Samples(1));
}

TEST(FridgeLFOSystemTest, RootConfigChangesRebaseTheCurrentVirtualConfig) {
  fridge::config::Config config;
  config.dry = 0.25f;
  config.lfos[0] = DeterministicLfo();
  config.lfos[0].targets[0] = Target{.object = TargetObject::kMixer,
                                     .parameter = TargetParameter::kDry};

  State system(1234);
  system.Reset(config);
  const fridge::config::Config& first_output =
      system.Update(config, Samples(1));
  EXPECT_FLOAT_EQ(first_output.dry, 1.25f);

  config.dry = 4.0f;
  const fridge::config::Config& rebased_output =
      system.Update(config, Samples(0));

  EXPECT_FLOAT_EQ(rebased_output.dry, 5.0f);
  EXPECT_EQ(system.time(), Samples(1));
}

TEST(FridgeLFOSystemTest, UpdateWithoutResetAutoInitializesFromRootConfig) {
  fridge::config::Config config;
  config.heads[0].read_amount = 0.5f;
  config.lfos[0] = DeterministicLfo();
  config.lfos[0].targets[0] = Target{.object = TargetObject::kHead,
                                     .parameter = TargetParameter::kReadAmount,
                                     .object_idx = 0};

  State system(1234);
  fridge::config::Config output = system.Update(config, Samples(1));

  EXPECT_FLOAT_EQ(output.heads[0].read_amount, 1.5f);
  EXPECT_EQ(system.time(), Samples(1));
}

TEST(FridgeLFOSystemTest, ResetSanitizesNonFiniteTargetedFloatValues) {
  fridge::config::Config config;
  config.dry = std::numeric_limits<float>::infinity();
  config.heads[0].feedback.amount = std::numeric_limits<float>::quiet_NaN();
  config.lfos[3].teleport_chance = std::numeric_limits<float>::infinity();
  config.lfos[4].reverse_chance = std::numeric_limits<float>::quiet_NaN();

  config.lfos[0] = DeterministicLfo();
  config.lfos[0].targets[0] = Target{.object = TargetObject::kMixer,
                                     .parameter = TargetParameter::kDry};
  config.lfos[1] = DeterministicLfo();
  config.lfos[1].targets[0] =
      Target{.object = TargetObject::kHead,
             .parameter = TargetParameter::kFeedbackAmount,
             .object_idx = 0};
  config.lfos[2] = DeterministicLfo();
  config.lfos[2].targets[0] =
      Target{.object = TargetObject::kLFO,
             .parameter = TargetParameter::kTeleportChance,
             .object_idx = 3};

  State system(1234);
  fridge::config::Config output = system.Reset(config);

  EXPECT_NEAR(output.dry, 0.0f, 0.1e-9);
  EXPECT_FLOAT_EQ(output.heads[0].feedback.amount, 0.0f);
  EXPECT_FLOAT_EQ(output.lfos[3].teleport_chance, 0.0f);
}

TEST(FridgeLFOSystemTest, UpdateClampsNonFiniteProbabilityInputsForEngines) {
  fridge::config::Config config;
  config.lfos[0] = DeterministicLfo();

  State system(1234);
  system.Reset(config);

  config.lfos[0].reverse_chance = std::numeric_limits<float>::quiet_NaN();
  config.lfos[0].teleport_chance = std::numeric_limits<float>::infinity();
  config.lfos[0].pitch_shift_chance = -std::numeric_limits<float>::infinity();
  config.lfos[0].low_octave_chance = std::numeric_limits<float>::quiet_NaN();
  config.lfos[0].high_octave_chance = std::numeric_limits<float>::infinity();

  fridge::config::Config output = system.Update(config, Samples(1));

  EXPECT_FLOAT_EQ(output.lfos[0].reverse_chance, 0.0f);
  EXPECT_FLOAT_EQ(output.lfos[0].teleport_chance, 0.0f);
  EXPECT_FLOAT_EQ(output.lfos[0].pitch_shift_chance, 0.0f);
  EXPECT_FLOAT_EQ(output.lfos[0].low_octave_chance, 0.0f);
  EXPECT_FLOAT_EQ(output.lfos[0].high_octave_chance, 0.0f);
}

TEST(FridgeLFOSystemTest, LfoCanModulateRemainingHeadTargets) {
  fridge::config::Config config;
  config.heads[0].read_amount = 0.25f;
  config.heads[1].erase_amount = 0.5f;
  config.heads[1].feedback.amount = 0.75f;

  config.lfos[0] = DeterministicLfo();
  config.lfos[0].targets[0] = Target{.object = TargetObject::kHead,
                                     .parameter = TargetParameter::kReadAmount,
                                     .object_idx = 0};
  config.lfos[0].targets[1] = Target{.object = TargetObject::kHead,
                                     .parameter = TargetParameter::kEraseAmount,
                                     .object_idx = 1};
  config.lfos[0].targets[2] =
      Target{.object = TargetObject::kHead,
             .parameter = TargetParameter::kFeedbackAmount,
             .object_idx = 1};

  State system(1234);
  system.Reset(config);
  fridge::config::Config output = system.Update(config, Samples(1));

  EXPECT_FLOAT_EQ(output.heads[0].read_amount, 1.25f);
  EXPECT_FLOAT_EQ(output.heads[1].erase_amount, 1.5f);
  EXPECT_FLOAT_EQ(output.heads[1].feedback.amount, 1.75f);
}

TEST(FridgeLFOSystemTest, LfoCanModulateRemainingLfoTargets) {
  fridge::config::Config config;
  config.lfos[1].max_grain_size = 2;
  config.lfos[1].min_grain_size = 3;
  config.lfos[1].reverse_chance = 0.25f;
  config.lfos[1].teleport_chance = 0.35f;
  config.lfos[1].pitch_shift_chance = 0.45f;
  config.lfos[1].low_octave_chance = 0.55f;
  config.lfos[1].high_octave_chance = 0.65f;

  config.lfos[0] = DeterministicLfo();
  config.lfos[0].targets[0] =
      Target{.object = TargetObject::kLFO,
             .parameter = TargetParameter::kMaxGrainSize,
             .object_idx = 1};
  config.lfos[0].targets[1] =
      Target{.object = TargetObject::kLFO,
             .parameter = TargetParameter::kMinGrainSize,
             .object_idx = 1};
  config.lfos[0].targets[2] =
      Target{.object = TargetObject::kLFO,
             .parameter = TargetParameter::kReverseChance,
             .object_idx = 1};
  config.lfos[0].targets[3] =
      Target{.object = TargetObject::kLFO,
             .parameter = TargetParameter::kTeleportChance,
             .object_idx = 1};
  config.lfos[0].targets[4] =
      Target{.object = TargetObject::kLFO,
             .parameter = TargetParameter::kPitchShiftChance,
             .object_idx = 1};
  config.lfos[0].targets[5] =
      Target{.object = TargetObject::kLFO,
             .parameter = TargetParameter::kLowOctaveChance,
             .object_idx = 1};
  config.lfos[0].targets[6] =
      Target{.object = TargetObject::kLFO,
             .parameter = TargetParameter::kHighOctaveChance,
             .object_idx = 1};

  State system(1234);
  system.Reset(config);
  fridge::config::Config output = system.Update(config, Samples(1));

  EXPECT_EQ(output.lfos[1].max_grain_size, 3u);
  EXPECT_EQ(output.lfos[1].min_grain_size, 4u);
  EXPECT_FLOAT_EQ(output.lfos[1].reverse_chance, 1.0f);
  EXPECT_FLOAT_EQ(output.lfos[1].teleport_chance, 1.0f);
  EXPECT_FLOAT_EQ(output.lfos[1].pitch_shift_chance, 1.0f);
  EXPECT_FLOAT_EQ(output.lfos[1].low_octave_chance, 1.0f);
  EXPECT_FLOAT_EQ(output.lfos[1].high_octave_chance, 1.0f);
}

TEST(FridgeLFOSystemTest, UnsupportedAndInvalidTargetsAreIgnored) {
  fridge::config::Config config;
  config.dry = 0.3f;
  config.heads[0].write_amount = 0.4f;

  config.lfos[0] = DeterministicLfo();
  config.lfos[0].targets[0] = Target{.object = TargetObject::kMixer,
                                     .parameter = TargetParameter::kDry};
  config.lfos[0].targets[1] = Target{.object = TargetObject::kHead,
                                     .parameter = TargetParameter::kDry,
                                     .object_idx = 0};
  config.lfos[0].targets[2] = Target{.object = TargetObject::kLFO,
                                     .parameter = TargetParameter::kDry,
                                     .object_idx = 1};
  config.lfos[0].targets[3] = Target{.object = TargetObject::kLFO,
                                     .parameter = TargetParameter::kRange,
                                     .object_idx = 99};

  config.lfos[1] = DeterministicLfo();
  config.lfos[1].range = 7;

  State system(1234);
  system.Reset(config);
  fridge::config::Config output = system.Update(config, Samples(1));

  EXPECT_FLOAT_EQ(output.dry, 1.3f);
  EXPECT_FLOAT_EQ(output.heads[0].write_amount, 0.4f);
  EXPECT_EQ(output.lfos[1].range, 7u);
}

TEST(FridgeLFOSystemTest, HeadPositionReverseCreatesTransitionEvent) {
  fridge::config::Config config;
  config.heads[0].position = 10;
  config.lfos[0] = LFO{.range = 20,
                       .max_grain_size = 1,
                       .min_grain_size = 1,
                       .reverse_chance = 1.0f};
  config.lfos[0].targets[0] = Target{.object = TargetObject::kHead,
                                     .parameter = TargetParameter::kPosition,
                                     .object_idx = 0};

  State system(1234);
  system.Reset(config);
  fridge::config::Config output = system.Update(config, Samples(1));
  const auto& transitions = system.head_transitions();

  ASSERT_TRUE(transitions[0].has_value());
  EXPECT_TRUE(transitions[0]->reversed);
  EXPECT_FALSE(transitions[0]->teleported);
  EXPECT_EQ(transitions[0]->old_motion.position, 10u);
  EXPECT_EQ(transitions[0]->new_motion.position, output.heads[0].position);
}

TEST(FridgeLFOSystemTest, NonHeadPositionLfoTransitionIsNotRecorded) {
  fridge::config::Config config;
  config.lfos[0] = LFO{.range = 20,
                       .max_grain_size = 1,
                       .min_grain_size = 1,
                       .reverse_chance = 1.0f};
  config.lfos[0].targets[0] = Target{.object = TargetObject::kMixer,
                                     .parameter = TargetParameter::kDry};

  State system(1234);
  system.Reset(config);
  system.Update(config, Samples(1));

  for (const auto& transition : system.head_transitions()) {
    EXPECT_FALSE(transition.has_value());
  }
}
