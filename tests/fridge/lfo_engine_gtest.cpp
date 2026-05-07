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

TEST(FridgeLFOTest, TickAdvancesAndWrapsToRange) {
  LFO config = DeterministicLfo(2);
  LFOEngine engine(config, 1234);

  EXPECT_FLOAT_EQ(engine.Tick(1.5f), 1.5f);
  EXPECT_FLOAT_EQ(engine.Tick(1.5f), 1.0f);
  EXPECT_FLOAT_EQ(engine.value(), 1.0f);
}

TEST(FridgeLFOTest, BackwardMotionWrapsAroundRange) {
  LFO config = DeterministicLfo(10, 4);
  LFOEngine engine(config, 1234);

  engine.Reset(0.0f, Direction::kBackwards);

  EXPECT_FLOAT_EQ(engine.Tick(1.0f), 9.0f);
}

TEST(FridgeLFOTest, ZeroRangeStaysAtZero) {
  LFO config = DeterministicLfo(0, 4);
  LFOEngine engine(config, 1234);

  engine.Reset(5.0f, Direction::kForwards);

  EXPECT_FLOAT_EQ(engine.value(), 0.0f);
  EXPECT_FLOAT_EQ(engine.Tick(1.0f), 0.0f);
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

TEST(FridgeLFOTest, ResetWrapsInitialValueAndPreservesDirection) {
  LFO config = DeterministicLfo(3, 2);
  LFOEngine engine(config, 1234);

  engine.Reset(99.0f, Direction::kBackwards);

  EXPECT_FLOAT_EQ(engine.value(), 0.0f);
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

TEST(FridgeLFOTest, SetConfigWrapsCurrentValueToNewRange) {
  LFO initial_config = DeterministicLfo(10, 2);
  LFOEngine engine(initial_config, 1234);
  ASSERT_FLOAT_EQ(engine.Tick(7.0f), 7.0f);

  LFO smaller_range = DeterministicLfo(4, 2);
  engine.SetConfig(smaller_range);

  EXPECT_FLOAT_EQ(engine.value(), 3.0f);
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

TEST(FridgeLFOTest, TickWithEventsReportsReverseTransition) {
  LFO config{.range = 10,
             .max_grain_size = 1,
             .min_grain_size = 1,
             .reverse_chance = 1.0f};
  LFOEngine engine(config, 1234);

  fridge::state::LFOTickResult result = engine.TickWithEvents(1.0f);

  ASSERT_TRUE(result.transition.has_value());
  EXPECT_TRUE(result.transition->reversed);
  EXPECT_FALSE(result.transition->teleported);
  EXPECT_EQ(result.transition->old_direction, Direction::kForwards);
  EXPECT_EQ(result.transition->new_direction, Direction::kBackwards);
}

TEST(FridgeLFOTest, TickWithEventsAccumulatesTransitionsAcrossTick) {
  LFO config{.range = 10,
             .max_grain_size = 1,
             .min_grain_size = 1,
             .reverse_chance = 1.0f};
  LFOEngine engine(config, 1234);

  fridge::state::LFOTickResult result = engine.TickWithEvents(2.0f);

  ASSERT_TRUE(result.transition.has_value());
  EXPECT_TRUE(result.transition->reversed);
  EXPECT_EQ(result.transition->old_direction, Direction::kForwards);
  EXPECT_EQ(result.transition->new_direction, Direction::kForwards);
}

TEST(FridgeLFOTest, TickWithEventsReportsTeleportTransition) {
  LFO config{.range = 10,
             .max_grain_size = 1,
             .min_grain_size = 1,
             .teleport_chance = 1.0f};
  LFOEngine engine(config, 1234);

  fridge::state::LFOTickResult result = engine.TickWithEvents(1.0f);

  ASSERT_TRUE(result.transition.has_value());
  EXPECT_FALSE(result.transition->reversed);
  EXPECT_TRUE(result.transition->teleported);
  EXPECT_NE(result.transition->old_value, result.transition->new_value);
}
