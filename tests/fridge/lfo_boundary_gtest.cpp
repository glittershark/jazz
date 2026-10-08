#include <cmath>
#include <limits>

#include "gtest/gtest.h"
#include "lfo.hpp"
#include "mod.hpp"

using namespace fridge;
using jazz::units::Samples;

TEST(LfoBoundaryTest, FullWidthRandomIntervalDoesNotDivideByZero) {
  mod::Rng rng(987);
  uint32_t different = rng.Between(0, UINT32_MAX);
  bool changed = false;
  for (int i = 0; i < 1000; ++i) {
    changed |= rng.Between(0, UINT32_MAX) != different;
    EXPECT_EQ(rng.Between(7, 7), 7);
    EXPECT_EQ(rng.Between(9, 2), 9);
  }
  EXPECT_TRUE(changed);
}

TEST(LfoBoundaryTest, OversizedGrainsCannotTruncateToZeroAndHang) {
  config::LFO config;
  config.range = 100;
  config.min_grain_size = std::numeric_limits<size_t>::max();
  config.max_grain_size = std::numeric_limits<size_t>::max();
  mod::LFOEngine engine(config);
  EXPECT_EQ(engine.grain_size(), UINT32_MAX);
  EXPECT_FLOAT_EQ(engine.Tick(Samples(1u)), 1);
}

TEST(LfoBoundaryTest, NonfiniteResetsRemainFinite) {
  mod::LFOEngine engine(config::LFO{.range = 100});
  for (float value : {NAN, -NAN, INFINITY, -INFINITY}) {
    engine.Reset(value);
    EXPECT_FLOAT_EQ(engine.value(), 0);
    EXPECT_FLOAT_EQ(engine.Tick(Samples(1u)), 1);
  }
}

TEST(LfoBoundaryTest,
     InvalidProbabilitiesAndSizesAreSanitizedAtEngineBoundary) {
  mod::LFOEngine engine;
  engine.SetParams({.range = SIZE_MAX,
                    .max_grain_size = 0,
                    .min_grain_size = 0,
                    .reverse_chance = NAN,
                    .teleport_chance = INFINITY,
                    .pitch_shift_chance = -1,
                    .low_octave_chance = 4,
                    .high_octave_chance = -INFINITY});
  EXPECT_EQ(engine.params().range, kBufferLen);
  EXPECT_EQ(engine.params().min_grain_size, 1);
  EXPECT_EQ(engine.params().max_grain_size, 1);
  EXPECT_FLOAT_EQ(engine.params().reverse_chance, 0);
  EXPECT_FLOAT_EQ(engine.params().teleport_chance, 0);
  EXPECT_FLOAT_EQ(engine.params().low_octave_chance, 1);
  engine.Reset();
  EXPECT_FLOAT_EQ(engine.Tick(Samples(10u)), 10);
}

TEST(LfoBoundaryTest, ClampSizeIsDefinedAcrossSeedIntegerBoundaries) {
  for (float value : {NAN, INFINITY, -INFINITY, -100.0f}) {
    EXPECT_EQ(mod::Modulator::ClampSize(value, 1), 1);
  }
  EXPECT_EQ(mod::Modulator::ClampSize(0x1p31f, 0), 2147483648u);
  EXPECT_EQ(mod::Modulator::ClampSize(0x1p32f, 0), UINT32_MAX);
  EXPECT_EQ(mod::Modulator::ClampSize(std::numeric_limits<float>::max(), 0),
            UINT32_MAX);
  EXPECT_EQ(mod::Modulator::ClampSize(2.5f, 0), 3);
}

class LfoSeedTest : public testing::TestWithParam<uint32_t> {};

TEST_P(LfoSeedTest, BatchedAndSingleSampleTicksHaveIdenticalRandomEvolution) {
  config::LFO config{.range = 32768,
                     .max_grain_size = 31,
                     .min_grain_size = 1,
                     .reverse_chance = 0.37f,
                     .teleport_chance = 0.29f,
                     .pitch_shift_chance = 0.51f,
                     .low_octave_chance = 0.3f,
                     .high_octave_chance = 0.7f};
  mod::LFOEngine batched(config, GetParam());
  mod::LFOEngine single(config, GetParam());
  for (uint32_t batch = 1; batch <= 64; ++batch) {
    const float value = batched.Tick(Samples(batch));
    for (uint32_t sample = 0; sample < batch; ++sample) {
      single.Tick(Samples(1u));
    }
    EXPECT_NEAR(value, single.value(), 0.01f);
    EXPECT_EQ(batched.direction(), single.direction());
    EXPECT_FLOAT_EQ(batched.speed(), single.speed());
    EXPECT_EQ(batched.grain_size(), single.grain_size());
    EXPECT_EQ(batched.grain_time_remaining(), single.grain_time_remaining());
    ASSERT_GE(value, 0);
    ASSERT_LT(value, config.range);
  }
}

TEST_P(LfoSeedTest, PhaseResizeDoesNotChangeRandomSequenceOrGrainTiming) {
  config::LFO config{.range = 1000,
                     .max_grain_size = 71,
                     .min_grain_size = 11,
                     .reverse_chance = 0.5f,
                     .pitch_shift_chance = 0.6f,
                     .low_octave_chance = 0.3f,
                     .high_octave_chance = 0.7f};
  mod::LFOEngine stable(config, GetParam()), resized(config, GetParam());
  for (uint32_t i = 0; i < 2048; ++i) {
    if (i % 37 == 0) {
      auto params = resized.params();
      params.range = 100 + i;
      resized.SetParams(params, true);
    }
    stable.Tick(Samples(1u));
    resized.Tick(Samples(1u));
    EXPECT_EQ(stable.direction(), resized.direction());
    EXPECT_EQ(stable.speed(), resized.speed());
    EXPECT_EQ(stable.grain_time_remaining(), resized.grain_time_remaining());
    ASSERT_TRUE(std::isfinite(resized.value()));
    ASSERT_GE(resized.value(), 0);
    ASSERT_LT(resized.value(), resized.params().range);
  }
}

INSTANTIATE_TEST_SUITE_P(DeterministicSeeds, LfoSeedTest,
                         testing::Values(0u, 1u, 2u, 17u, 127u, 1234u, 65535u,
                                         0x80000000u, UINT32_MAX));

TEST(LfoBoundaryTest, ModulatedRangeReportsTheSameLimitAsTheEngine) {
  config::Config config;
  config.routing = config::Routing::kAssignable;
  config.lfos[0].range = 100;
  config.lfos[1].range = kBufferLen;
  config.lfos[0].targets[0] =
      config::Target{.object = config::TargetObject::kLFO,
                     .parameter = config::TargetParameter::kRange,
                     .object_idx = 1};
  mod::Modulator modulator;
  EXPECT_EQ(modulator.Update(config, Samples(10u)).lfos[1].range, kBufferLen);
}

TEST(LfoBoundaryTest, RoutingChangesDiscardFadesFromThePreviousAddressSpace) {
  config::Config config;
  config.routing = config::Routing::kAssignable;
  config.heads[0].position = kBufferLen - 1;
  mod::Modulator modulator;
  modulator.Reset(config);
  config.routing = config::Routing::kPairedRegions;
  modulator.SetConfig(config);
  for (const auto& fade : modulator.fades()) {
    EXPECT_EQ(fade.remaining, 0);
  }
  EXPECT_EQ(modulator.TickSample().head_count, kNumHeads);
}
