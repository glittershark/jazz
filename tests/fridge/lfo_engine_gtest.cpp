#include "fridge.hpp"
#include "gtest/gtest.h"

using fridge::config::LFO;
using fridge::state::Direction;
using fridge::state::LFOEngine;

namespace {

LFO DeterministicLfo(size_t range = 10, size_t grain_size = 4) {
  return LFO{.range = range,
             .max_grain_size = grain_size,
             .min_grain_size = grain_size};
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
  LFO config{.range = 10,
             .max_grain_size = 2,
             .min_grain_size = 2,
             .reverse_chance = 1.0f};
  LFOEngine engine(config, 1234);

  EXPECT_FLOAT_EQ(engine.Tick(2.0f), 2.0f);
  EXPECT_EQ(engine.direction(), Direction::kBackwards);
  EXPECT_FLOAT_EQ(engine.Tick(1.0f), 1.0f);
}

TEST(FridgeLFOTest, GrainBoundaryCanTeleport) {
  LFO config{.range = 10,
             .max_grain_size = 1,
             .min_grain_size = 1,
             .teleport_chance = 1.0f};
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
  LFO low_config{.range = 10,
                 .max_grain_size = 4,
                 .min_grain_size = 4,
                 .pitch_shift_chance = 1.0f,
                 .low_octave_chance = 1.0f};
  LFOEngine low_engine(low_config, 1234);
  EXPECT_FLOAT_EQ(low_engine.speed(), 0.5f);

  LFO high_config{.range = 10,
                  .max_grain_size = 4,
                  .min_grain_size = 4,
                  .pitch_shift_chance = 1.0f,
                  .high_octave_chance = 1.0f};
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
  EXPECT_EQ(engine.config().range, 4u);
}

TEST(FridgeLFOTest, ReverseFromBackwardDirectionCanFlipForwardAgain) {
  LFO config{.range = 10,
             .max_grain_size = 1,
             .min_grain_size = 1,
             .reverse_chance = 1.0f};
  LFOEngine engine(config, 1234);

  engine.Reset(5.0f, Direction::kBackwards);

  EXPECT_FLOAT_EQ(engine.Tick(1.0f), 4.0f);
  EXPECT_EQ(engine.direction(), Direction::kForwards);
}

TEST(FridgeLFOTest, PitchShiftFallsBackToUnityWhenOctaveWeightsAreZero) {
  LFO config{.range = 10,
             .max_grain_size = 4,
             .min_grain_size = 4,
             .pitch_shift_chance = 1.0f};
  LFOEngine engine(config, 1234);

  EXPECT_FLOAT_EQ(engine.speed(), 1.0f);
}
