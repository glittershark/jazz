#include <limits>

#include "fridge.hpp"
#include "gtest/gtest.h"

using fridge::config::LFO;
using fridge::config::Target;
using fridge::config::TargetObject;
using fridge::config::TargetParameter;
using fridge::state::Direction;
using fridge::state::LFOEngine;
using fridge::transform::state;

namespace {

LFO DeterministicLfo(size_t range = 10, size_t grain_size = 4) {
  return LFO(range, grain_size, grain_size, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
}

}  // namespace

TEST(FridgeLFOTest, StartsWithSampledGrainAndUnitySpeedByDefault) {
  LFO config = DeterministicLfo(8, 3);
  LFOEngine engine(config, 1234);

  EXPECT_FLOAT_EQ(engine.value(), 0.0f);
  EXPECT_EQ(engine.direction(), Direction::kForwards);
  EXPECT_EQ(engine.grain_size(), 3u);
  EXPECT_FLOAT_EQ(engine.grain_time_remaining(), 3.0f);
  EXPECT_FLOAT_EQ(engine.speed(), 1.0f);
}

TEST(FridgeLFOTest, TickAdvancesAndClampsToRange) {
  LFO config = DeterministicLfo(2);
  LFOEngine engine(config, 1234);

  EXPECT_FLOAT_EQ(engine.Tick(1.5f), 1.5f);
  EXPECT_FLOAT_EQ(engine.Tick(1.5f), 2.0f);
  EXPECT_FLOAT_EQ(engine.value(), 2.0f);
}

TEST(FridgeLFOTest, GrainBoundaryCanReverseDirection) {
  LFO config(10, 2, 2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
  LFOEngine engine(config, 1234);

  EXPECT_FLOAT_EQ(engine.Tick(2.0f), 2.0f);
  EXPECT_EQ(engine.direction(), Direction::kBackwards);
  EXPECT_FLOAT_EQ(engine.Tick(1.0f), 1.0f);
}

TEST(FridgeLFOTest, GrainBoundaryCanTeleport) {
  LFO config(10, 1, 1, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f);
  LFOEngine engine(config, 1234);

  float before_boundary = engine.Tick(0.5f);
  float after_boundary = engine.Tick(0.5f);

  EXPECT_GE(before_boundary, 0.0f);
  EXPECT_LE(before_boundary, 10.0f);
  EXPECT_GE(after_boundary, 0.0f);
  EXPECT_LE(after_boundary, 10.0f);
  EXPECT_NE(after_boundary, before_boundary);
}

TEST(FridgeLFOTest, PitchShiftUsesLowAndHighOctaveWeights) {
  LFO low_config(10, 4, 4, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f);
  LFOEngine low_engine(low_config, 1234);
  EXPECT_FLOAT_EQ(low_engine.speed(), 0.5f);

  LFO high_config(10, 4, 4, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
  LFOEngine high_engine(high_config, 1234);
  EXPECT_FLOAT_EQ(high_engine.speed(), 2.0f);
}

TEST(FridgeLFOTest, DefaultSeedConstructorStillHonorsFixedGrainAndSpeed) {
  LFO config = DeterministicLfo(8, 3);
  LFOEngine engine(config);

  EXPECT_FLOAT_EQ(engine.value(), 0.0f);
  EXPECT_EQ(engine.grain_size(), 3u);
  EXPECT_FLOAT_EQ(engine.speed(), 1.0f);
}

TEST(FridgeLFOTest, LargeDtCarriesAcrossMultipleGrains) {
  LFO config = DeterministicLfo(20, 2);
  LFOEngine engine(config, 1234);

  EXPECT_FLOAT_EQ(engine.Tick(5.0f), 5.0f);
  EXPECT_EQ(engine.grain_size(), 2u);
  EXPECT_FLOAT_EQ(engine.speed(), 1.0f);
}

TEST(FridgeLFOTest, ResetClampsInitialValueAndPreservesDirection) {
  LFO config = DeterministicLfo(3, 2);
  LFOEngine engine(config, 1234);

  engine.Reset(99.0f, Direction::kBackwards);

  EXPECT_FLOAT_EQ(engine.value(), 3.0f);
  EXPECT_EQ(engine.direction(), Direction::kBackwards);
  EXPECT_EQ(engine.grain_size(), 2u);
}

TEST(FridgeLFOTest, NonPositiveDtDoesNotAdvanceState) {
  LFO config = DeterministicLfo(8, 3);
  LFOEngine engine(config, 1234);

  EXPECT_FLOAT_EQ(engine.Tick(0.0f), 0.0f);
  EXPECT_FLOAT_EQ(engine.Tick(-4.0f), 0.0f);
  EXPECT_FLOAT_EQ(engine.value(), 0.0f);
  EXPECT_FLOAT_EQ(engine.grain_time_remaining(), 3.0f);
}

TEST(FridgeLFOTest, SetConfigClampsCurrentValueToNewRange) {
  LFO initial_config = DeterministicLfo(10, 2);
  LFOEngine engine(initial_config, 1234);
  ASSERT_FLOAT_EQ(engine.Tick(7.0f), 7.0f);

  LFO smaller_range = DeterministicLfo(4, 2);
  engine.SetConfig(smaller_range);

  EXPECT_FLOAT_EQ(engine.value(), 4.0f);
  EXPECT_EQ(engine.config().range(), 4u);
}

TEST(FridgeLFOTest, ReverseFromBackwardDirectionCanFlipForwardAgain) {
  LFO config(10, 1, 1, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
  LFOEngine engine(config, 1234);

  engine.Reset(5.0f, Direction::kBackwards);

  EXPECT_FLOAT_EQ(engine.Tick(1.0f), 4.0f);
  EXPECT_EQ(engine.direction(), Direction::kForwards);
}

TEST(FridgeLFOTest, PitchShiftFallsBackToUnityWhenOctaveWeightsAreZero) {
  LFO config(10, 4, 4, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
  LFOEngine engine(config, 1234);

  EXPECT_FLOAT_EQ(engine.speed(), 1.0f);
}

TEST(FridgeLFOSystemTest, ResetStartsAtStaticKnobPositions) {
  fridge::config::Config config;
  config.heads()[0].position() = 12;
  config.heads()[0].write_amount() = 0.25f;
  config.heads()[0].feedback().amount() = 0.75f;
  config.lfos()[0].range() = 10;
  config.lfos()[0].max_grain_size() = 4;
  config.lfos()[0].min_grain_size() = 2;
  config.dry() = 0.3f;
  config.wet() = 0.9f;

  state system(1234);
  fridge::config::Config output = system.Reset(config);

  EXPECT_EQ(output.heads()[0].position(), 12u);
  EXPECT_FLOAT_EQ(output.heads()[0].write_amount(), 0.25f);
  EXPECT_FLOAT_EQ(output.heads()[0].feedback().amount(), 0.75f);
  EXPECT_EQ(output.lfos()[0].range(), 10u);
  EXPECT_EQ(output.lfos()[0].max_grain_size(), 4u);
  EXPECT_FLOAT_EQ(output.dry(), 0.3f);
  EXPECT_FLOAT_EQ(output.wet(), 0.9f);
  EXPECT_FLOAT_EQ(system.time(), 0.0f);
}

TEST(FridgeLFOSystemTest, ResetAndUpdateReturnStableConfigReference) {
  fridge::config::Config config;
  config.lfos()[0] = DeterministicLfo();
  config.lfos()[0].targets()[0] =
      Target(TargetObject::kMixer, TargetParameter::kDry);

  state system(1234);
  const fridge::config::Config* reset_output = &system.Reset(config);
  const fridge::config::Config* paused_output = &system.Update(config, 0.0f);

  EXPECT_EQ(reset_output, paused_output);
  EXPECT_FLOAT_EQ(paused_output->dry(), 1.0f);
}

TEST(FridgeLFOSystemTest, UpdateCreatesVirtualHeadKnobPositions) {
  fridge::config::Config config;
  config.heads()[0].write_amount() = 0.25f;
  config.lfos()[0] = DeterministicLfo();
  config.lfos()[0].targets()[0] =
      Target(TargetObject::kHead, TargetParameter::kWriteAmount, 0);

  state system(1234);
  fridge::config::Config initial = system.Reset(config);

  EXPECT_FLOAT_EQ(initial.heads()[0].write_amount(), 0.25f);

  fridge::config::Config output = system.Update(config, 1.5f);

  EXPECT_FLOAT_EQ(config.heads()[0].write_amount(), 0.25f);
  EXPECT_FLOAT_EQ(output.heads()[0].write_amount(), 1.75f);
  EXPECT_FLOAT_EQ(system.time(), 1.5f);
}

TEST(FridgeLFOSystemTest, LfoCanModulateMixerKnobs) {
  fridge::config::Config config;
  config.dry() = 0.2f;
  config.wet() = 0.4f;
  config.lfos()[0] = DeterministicLfo();
  config.lfos()[0].targets()[0] =
      Target(TargetObject::kMixer, TargetParameter::kDry);
  config.lfos()[0].targets()[1] =
      Target(TargetObject::kMixer, TargetParameter::kWet);

  state system(1234);
  system.Reset(config);
  fridge::config::Config output = system.Update(config, 1.25f);

  EXPECT_FLOAT_EQ(output.dry(), 1.45f);
  EXPECT_FLOAT_EQ(output.wet(), 1.65f);
}

TEST(FridgeLFOSystemTest, LfoModulatesAnotherLfoOnTheNextTick) {
  fridge::config::Config config;
  config.dry() = 0.0f;
  config.lfos()[0] = DeterministicLfo(2, 2);
  config.lfos()[0].targets()[0] =
      Target(TargetObject::kLFO, TargetParameter::kReverseChance, 1);
  config.lfos()[1] = DeterministicLfo(10, 1);
  config.lfos()[1].targets()[0] =
      Target(TargetObject::kMixer, TargetParameter::kDry);

  state system(1234);
  fridge::config::Config initial = system.Reset(config);

  EXPECT_FLOAT_EQ(initial.lfos()[1].reverse_chance(), 0.0f);

  fridge::config::Config first_output = system.Update(config, 1.0f);

  EXPECT_FLOAT_EQ(first_output.lfos()[1].reverse_chance(), 1.0f);
  EXPECT_FLOAT_EQ(first_output.dry(), 1.0f);

  fridge::config::Config second_output = system.Update(config, 1.0f);
  fridge::config::Config third_output = system.Update(config, 1.0f);

  EXPECT_FLOAT_EQ(second_output.dry(), 2.0f);
  EXPECT_FLOAT_EQ(third_output.dry(), 1.0f);
}

TEST(FridgeLFOSystemTest, ResetRestoresStaticVirtualState) {
  fridge::config::Config config;
  config.heads()[0].position() = 9;
  config.lfos()[0] = DeterministicLfo();
  config.lfos()[0].targets()[0] =
      Target(TargetObject::kHead, TargetParameter::kPosition, 0);

  state system(1234);
  system.Reset(config);
  fridge::config::Config modulated = system.Update(config, 2.0f);
  ASSERT_NE(modulated.heads()[0].position(), 9u);

  fridge::config::Config reset = system.Reset(config);

  EXPECT_EQ(reset.heads()[0].position(), 9u);
  EXPECT_FLOAT_EQ(system.time(), 0.0f);
}

TEST(FridgeLFOSystemTest, InvalidTargetsAreIgnored) {
  fridge::config::Config config;
  config.heads()[0].write_amount() = 0.25f;
  config.lfos()[0] = DeterministicLfo();
  config.lfos()[0].targets()[0] =
      Target(TargetObject::kHead, TargetParameter::kWriteAmount, 99);
  config.lfos()[0].targets()[1] =
      Target(TargetObject::kMixer, TargetParameter::kWriteAmount);

  state system(1234);
  system.Reset(config);
  fridge::config::Config output = system.Update(config, 2.0f);

  EXPECT_FLOAT_EQ(output.heads()[0].write_amount(), 0.25f);
}

TEST(FridgeLFOSystemTest, ModulatedLfoRangeIsClampedForNextTick) {
  fridge::config::Config config;
  config.dry() = 0.0f;
  config.lfos()[0] = DeterministicLfo(2, 2);
  config.lfos()[0].targets()[0] =
      Target(TargetObject::kLFO, TargetParameter::kRange, 1);
  config.lfos()[1] = DeterministicLfo(0, 1);
  config.lfos()[1].targets()[0] =
      Target(TargetObject::kMixer, TargetParameter::kDry);

  state system(1234);
  fridge::config::Config initial = system.Reset(config);
  EXPECT_EQ(initial.lfos()[1].range(), 0u);

  fridge::config::Config first_output = system.Update(config, 1.0f);
  EXPECT_EQ(first_output.lfos()[1].range(), 1u);

  fridge::config::Config second_output = system.Update(config, 1.0f);
  EXPECT_FLOAT_EQ(second_output.dry(), 1.0f);
}

TEST(FridgeLFOSystemTest, NonPositiveDtReturnsCurrentVirtualConfig) {
  fridge::config::Config config;
  config.lfos()[0] = DeterministicLfo();
  config.lfos()[0].targets()[0] =
      Target(TargetObject::kMixer, TargetParameter::kDry);

  state system(1234);
  system.Reset(config);
  fridge::config::Config advanced = system.Update(config, 1.5f);
  fridge::config::Config paused = system.Update(config, 0.0f);

  EXPECT_FLOAT_EQ(paused.dry(), advanced.dry());
  EXPECT_FLOAT_EQ(system.time(), 1.5f);
}

TEST(FridgeLFOSystemTest, RootConfigChangesRebaseTheCurrentVirtualConfig) {
  fridge::config::Config config;
  config.dry() = 0.25f;
  config.lfos()[0] = DeterministicLfo();
  config.lfos()[0].targets()[0] =
      Target(TargetObject::kMixer, TargetParameter::kDry);

  state system(1234);
  system.Reset(config);
  const fridge::config::Config& first_output = system.Update(config, 1.5f);
  EXPECT_FLOAT_EQ(first_output.dry(), 1.75f);

  config.dry() = 4.0f;
  const fridge::config::Config& rebased_output = system.Update(config, 0.0f);

  EXPECT_FLOAT_EQ(rebased_output.dry(), 5.5f);
  EXPECT_FLOAT_EQ(system.time(), 1.5f);
}

TEST(FridgeLFOSystemTest, UpdateWithoutResetAutoInitializesFromRootConfig) {
  fridge::config::Config config;
  config.heads()[0].read_amount() = 0.5f;
  config.lfos()[0] = DeterministicLfo();
  config.lfos()[0].targets()[0] =
      Target(TargetObject::kHead, TargetParameter::kReadAmount, 0);

  state system(1234);
  fridge::config::Config output = system.Update(config, 1.0f);

  EXPECT_FLOAT_EQ(output.heads()[0].read_amount(), 1.5f);
  EXPECT_FLOAT_EQ(system.time(), 1.0f);
}

TEST(FridgeLFOSystemTest, ResetSanitizesNonFiniteTargetedFloatValues) {
  fridge::config::Config config;
  config.dry() = std::numeric_limits<float>::infinity();
  config.heads()[0].feedback().amount() =
      std::numeric_limits<float>::quiet_NaN();
  config.lfos()[3].teleport_chance() = std::numeric_limits<float>::infinity();
  config.lfos()[4].reverse_chance() = std::numeric_limits<float>::quiet_NaN();

  config.lfos()[0] = DeterministicLfo();
  config.lfos()[0].targets()[0] =
      Target(TargetObject::kMixer, TargetParameter::kDry);
  config.lfos()[1] = DeterministicLfo();
  config.lfos()[1].targets()[0] =
      Target(TargetObject::kHead, TargetParameter::kFeedbackAmount, 0);
  config.lfos()[2] = DeterministicLfo();
  config.lfos()[2].targets()[0] =
      Target(TargetObject::kLFO, TargetParameter::kTeleportChance, 3);

  state system(1234);
  fridge::config::Config output = system.Reset(config);

  EXPECT_FLOAT_EQ(output.dry(), 0.0f);
  EXPECT_FLOAT_EQ(output.heads()[0].feedback().amount(), 0.0f);
  EXPECT_FLOAT_EQ(output.lfos()[3].teleport_chance(), 0.0f);
  EXPECT_FLOAT_EQ(output.lfos()[4].reverse_chance(), 0.0f);
}

TEST(FridgeLFOSystemTest, UpdateClampsNonFiniteProbabilityInputsForEngines) {
  fridge::config::Config config;
  config.lfos()[0] = DeterministicLfo();

  state system(1234);
  system.Reset(config);

  config.lfos()[0].reverse_chance() = std::numeric_limits<float>::quiet_NaN();
  config.lfos()[0].teleport_chance() = std::numeric_limits<float>::infinity();
  config.lfos()[0].pitch_shift_chance() =
      -std::numeric_limits<float>::infinity();
  config.lfos()[0].low_octave_chance() =
      std::numeric_limits<float>::quiet_NaN();
  config.lfos()[0].high_octave_chance() =
      std::numeric_limits<float>::infinity();

  fridge::config::Config output = system.Update(config, 1.0f);

  EXPECT_FLOAT_EQ(output.lfos()[0].reverse_chance(), 0.0f);
  EXPECT_FLOAT_EQ(output.lfos()[0].teleport_chance(), 0.0f);
  EXPECT_FLOAT_EQ(output.lfos()[0].pitch_shift_chance(), 0.0f);
  EXPECT_FLOAT_EQ(output.lfos()[0].low_octave_chance(), 0.0f);
  EXPECT_FLOAT_EQ(output.lfos()[0].high_octave_chance(), 0.0f);
}

TEST(FridgeLFOSystemTest, LfoCanModulateRemainingHeadTargets) {
  fridge::config::Config config;
  config.heads()[0].read_amount() = 0.25f;
  config.heads()[1].erase_amount() = 0.5f;
  config.heads()[2].feedback().amount() = 0.75f;

  config.lfos()[0] = DeterministicLfo();
  config.lfos()[0].targets()[0] =
      Target(TargetObject::kHead, TargetParameter::kReadAmount, 0);
  config.lfos()[1] = DeterministicLfo();
  config.lfos()[1].targets()[0] =
      Target(TargetObject::kHead, TargetParameter::kEraseAmount, 1);
  config.lfos()[2] = DeterministicLfo();
  config.lfos()[2].targets()[0] =
      Target(TargetObject::kHead, TargetParameter::kFeedbackAmount, 2);

  state system(1234);
  system.Reset(config);
  fridge::config::Config output = system.Update(config, 1.0f);

  EXPECT_FLOAT_EQ(output.heads()[0].read_amount(), 1.25f);
  EXPECT_FLOAT_EQ(output.heads()[1].erase_amount(), 1.5f);
  EXPECT_FLOAT_EQ(output.heads()[2].feedback().amount(), 1.75f);
}

TEST(FridgeLFOSystemTest, LfoCanModulateRemainingLfoTargets) {
  fridge::config::Config config;
  config.lfos()[7].max_grain_size() = 2;
  config.lfos()[7].min_grain_size() = 3;
  config.lfos()[7].reverse_chance() = 0.25f;
  config.lfos()[7].teleport_chance() = 0.35f;
  config.lfos()[7].pitch_shift_chance() = 0.45f;
  config.lfos()[7].low_octave_chance() = 0.55f;
  config.lfos()[7].high_octave_chance() = 0.65f;

  for (size_t i = 0; i < 7; ++i) {
    config.lfos()[i] = DeterministicLfo();
  }

  config.lfos()[0].targets()[0] =
      Target(TargetObject::kLFO, TargetParameter::kMaxGrainSize, 7);
  config.lfos()[1].targets()[0] =
      Target(TargetObject::kLFO, TargetParameter::kMinGrainSize, 7);
  config.lfos()[2].targets()[0] =
      Target(TargetObject::kLFO, TargetParameter::kReverseChance, 7);
  config.lfos()[3].targets()[0] =
      Target(TargetObject::kLFO, TargetParameter::kTeleportChance, 7);
  config.lfos()[4].targets()[0] =
      Target(TargetObject::kLFO, TargetParameter::kPitchShiftChance, 7);
  config.lfos()[5].targets()[0] =
      Target(TargetObject::kLFO, TargetParameter::kLowOctaveChance, 7);
  config.lfos()[6].targets()[0] =
      Target(TargetObject::kLFO, TargetParameter::kHighOctaveChance, 7);

  state system(1234);
  system.Reset(config);
  fridge::config::Config output = system.Update(config, 1.0f);

  EXPECT_EQ(output.lfos()[7].max_grain_size(), 3u);
  EXPECT_EQ(output.lfos()[7].min_grain_size(), 4u);
  EXPECT_FLOAT_EQ(output.lfos()[7].reverse_chance(), 1.0f);
  EXPECT_FLOAT_EQ(output.lfos()[7].teleport_chance(), 1.0f);
  EXPECT_FLOAT_EQ(output.lfos()[7].pitch_shift_chance(), 1.0f);
  EXPECT_FLOAT_EQ(output.lfos()[7].low_octave_chance(), 1.0f);
  EXPECT_FLOAT_EQ(output.lfos()[7].high_octave_chance(), 1.0f);
}

TEST(FridgeLFOSystemTest, UnsupportedAndInvalidTargetsAreIgnored) {
  fridge::config::Config config;
  config.dry() = 0.3f;
  config.heads()[0].write_amount() = 0.4f;

  config.lfos()[0] = DeterministicLfo();
  config.lfos()[0].targets()[0] =
      Target(static_cast<TargetObject>(999), TargetParameter::kDry);
  config.lfos()[1] = DeterministicLfo();
  config.lfos()[1].targets()[0] =
      Target(TargetObject::kHead, TargetParameter::kDry, 0);
  config.lfos()[2] = DeterministicLfo();
  config.lfos()[2].range() = 7;
  config.lfos()[2].targets()[0] =
      Target(TargetObject::kLFO, TargetParameter::kDry, 2);
  config.lfos()[3] = DeterministicLfo();
  config.lfos()[3].targets()[0] =
      Target(TargetObject::kLFO, TargetParameter::kRange, 99);

  state system(1234);
  system.Reset(config);
  fridge::config::Config output = system.Update(config, 1.0f);

  EXPECT_FLOAT_EQ(output.dry(), 0.3f);
  EXPECT_FLOAT_EQ(output.heads()[0].write_amount(), 0.4f);
  EXPECT_EQ(output.lfos()[2].range(), 7u);
}
