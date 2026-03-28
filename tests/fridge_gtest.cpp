#include "fridge.hpp"
#include "gtest/gtest.h"

using fridge::config::Target;
using fridge::config::TargetObject;
using fridge::config::TargetParameter;
using fridge::config::LFO;
using fridge::state::Config;
using fridge::state::Direction;
using fridge::state::LFOEngine;

TEST(FridgeLFOTest, StartsWithSampledGrainAndUnitySpeedByDefault) {
  LFO config(8, 3, 3, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
  LFOEngine engine(config, 1234);

  EXPECT_FLOAT_EQ(engine.value(), 0.0f);
  EXPECT_EQ(engine.direction(), Direction::kForwards);
  EXPECT_EQ(engine.grain_size(), 3u);
  EXPECT_FLOAT_EQ(engine.grain_time_remaining(), 3.0f);
  EXPECT_FLOAT_EQ(engine.speed(), 1.0f);
}

TEST(FridgeLFOTest, TickAdvancesAndClampsToRange) {
  LFO config(2, 4, 4, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
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

  float before_boundary = engine.Tick(1.0f);
  float after_boundary = engine.value();

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

TEST(FridgeLFOTest, LargeDtCarriesAcrossMultipleGrains) {
  LFO config(20, 2, 2, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
  LFOEngine engine(config, 1234);

  EXPECT_FLOAT_EQ(engine.Tick(5.0f), 5.0f);
  EXPECT_EQ(engine.grain_size(), 2u);
  EXPECT_FLOAT_EQ(engine.speed(), 1.0f);
}

TEST(FridgeStateConfigTest, StartsAtStaticKnobPositions) {
  fridge::config::Config config;
  config.heads()[0].position() = 12;
  config.heads()[0].write_amount() = 0.25f;
  config.heads()[0].feedback().amount() = 0.75f;
  config.lfos()[0].range() = 10;
  config.lfos()[0].max_grain_size() = 4;
  config.lfos()[0].min_grain_size() = 2;
  config.dry() = 0.3f;
  config.wet() = 0.9f;

  Config state(config, 1234);

  EXPECT_FLOAT_EQ(state.heads()[0].position().static_value(), 12.0f);
  EXPECT_FLOAT_EQ(state.heads()[0].position().value(), 12.0f);
  EXPECT_FLOAT_EQ(state.heads()[0].write_amount().value(), 0.25f);
  EXPECT_FLOAT_EQ(state.heads()[0].feedback().amount().value(), 0.75f);
  EXPECT_FLOAT_EQ(state.lfos()[0].range().value(), 10.0f);
  EXPECT_FLOAT_EQ(state.lfos()[0].max_grain_size().value(), 4.0f);
  EXPECT_FLOAT_EQ(state.dry().value(), 0.3f);
  EXPECT_FLOAT_EQ(state.wet().value(), 0.9f);
  EXPECT_FLOAT_EQ(state.time(), 0.0f);
}

TEST(FridgeStateConfigTest, LfoCreatesVirtualHeadKnobPositions) {
  fridge::config::Config config;
  config.heads()[0].write_amount() = 0.25f;
  config.lfos()[0] = LFO(10, 4, 4, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
  config.lfos()[0].targets()[0] =
      Target(TargetObject::kHead, TargetParameter::kWriteAmount, 0);

  Config state(config, 1234);

  EXPECT_FLOAT_EQ(state.heads()[0].write_amount().value(), 0.25f);

  state.Tick(1.5f);

  EXPECT_FLOAT_EQ(state.heads()[0].write_amount().static_value(), 0.25f);
  EXPECT_FLOAT_EQ(state.heads()[0].write_amount().value(), 1.75f);
  EXPECT_FLOAT_EQ(state.time(), 1.5f);
}

TEST(FridgeStateConfigTest, LfoModulatesAnotherLfoOnTheNextTick) {
  fridge::config::Config config;
  config.lfos()[0] = LFO(2, 2, 2, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
  config.lfos()[0].targets()[0] =
      Target(TargetObject::kLFO, TargetParameter::kReverseChance, 1);
  config.lfos()[1] = LFO(10, 1, 1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

  Config state(config, 1234);

  EXPECT_FLOAT_EQ(state.lfos()[1].reverse_chance().value(), 0.0f);

  state.Tick(1.0f);

  EXPECT_FLOAT_EQ(state.lfos()[1].reverse_chance().value(), 1.0f);
  EXPECT_EQ(state.lfo_engines()[1].direction(), Direction::kForwards);

  state.Tick(1.0f);

  EXPECT_EQ(state.lfo_engines()[1].direction(), Direction::kBackwards);
}
