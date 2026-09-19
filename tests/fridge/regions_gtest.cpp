#include <cmath>
#include <limits>
#include <memory>

#include "config.hpp"
#include "gtest/gtest.h"
#include "mod.hpp"
#include "regions.hpp"
#include "sound.hpp"

using namespace fridge;
using jazz::audio::StereoSample;
using jazz::units::Samples;

TEST(FridgeRegionConfigTest, RejectsInvalidBudgetsAtomically) {
  config::Config config;
  const auto before = config;
  EXPECT_FALSE(config.ResizeRegion(0, 0));
  EXPECT_FALSE(config.ResizeRegion(0, std::numeric_limits<size_t>::max()));
  EXPECT_FALSE(config.ResizeRegion(0, kRegionCapacity));
  EXPECT_FALSE(config.ResizeRegion(kNumRegions, 100));
  EXPECT_FALSE(config.AssignRegion(kNumHeads, 0));
  EXPECT_FALSE(config.AssignRegion(0, kNumRegions));
  EXPECT_EQ(config, before);
}

TEST(FridgeRegionConfigTest, PageBudgetCanBeFilledAndReused) {
  config::Config config;
  for (auto& region : config.regions) {
    region.range = kRegionPageSize;
  }
  const size_t available =
      kRegionCapacity - (kNumRegions - 1) * kRegionPageSize;
  ASSERT_TRUE(config.ResizeRegion(0, available));
  EXPECT_FALSE(config.ResizeRegion(1, kRegionPageSize + 1));
  ASSERT_TRUE(config.ResizeRegion(0, available - kRegionPageSize));
  EXPECT_TRUE(config.ResizeRegion(1, kRegionPageSize + 1));
}

TEST(FridgeRegionMotionTest, PairedRoutingIgnoresButRetainsGeneralTargets) {
  config::Config config;
  for (auto& region : config.regions) {
    region.range = 100;
  }
  config.heads[9].region = 5;
  config.heads[9].position = 25;
  config.lfos[9].range = 3;  // Retained for assignable routing only.
  config.lfos[9].targets[0] = config::Target{
      .object = config::TargetObject::kMixer,
      .parameter = config::TargetParameter::kDry,
  };
  mod::Modulator modulator;
  const auto& output = modulator.Update(config, Samples(10));
  EXPECT_EQ(output.heads[9].position, 35);
  EXPECT_FLOAT_EQ(output.dry, config.dry);
  EXPECT_EQ(output.lfos[9].targets, config.lfos[9].targets);
  EXPECT_EQ(output.lfos[9].range, 3);
  EXPECT_NE(modulator.frame().regions, nullptr);
}

TEST(FridgeRegionMotionTest, ReassignmentPreservesActualMovingPosition) {
  config::Config config;
  config.regions[0].range = 100;
  config.regions[1].range = 200;
  config.heads[0].position = 25;
  mod::Modulator modulator;
  ASSERT_EQ(modulator.Update(config, Samples(10)).heads[0].position, 35);
  ASSERT_TRUE(config.AssignRegion(0, 1));
  const auto& reassigned = modulator.Update(config, Samples(0));
  EXPECT_EQ(reassigned.heads[0].region, 1);
  EXPECT_EQ(reassigned.heads[0].position, 70);
  // The crossfade retains the outgoing region and its local motion.
  EXPECT_EQ(modulator.fades()[0].old_head.region, 0);
  EXPECT_EQ(modulator.fades()[0].old_position, 35);
  EXPECT_EQ(modulator.Update(config, Samples(1)).heads[0].position, 71);
}

TEST(FridgeRegionMotionTest, SharedResizePreservesPhaseAndPlaybackSpeed) {
  config::Config config;
  config.regions[0].range = 100;
  config.heads[1].position = 50;
  config.lfos[1].min_grain_size = 1000;
  config.lfos[1].max_grain_size = 1000;
  mod::Modulator modulator;
  modulator.Update(config, Samples(10));
  ASSERT_TRUE(config.ResizeRegion(0, 200));
  const auto& resized = modulator.Update(config, Samples(0));
  EXPECT_EQ(resized.heads[0].position, 20);
  EXPECT_EQ(resized.heads[1].position, 120);
  const auto& moving = modulator.Update(config, Samples(1));
  EXPECT_EQ(moving.heads[0].position, 21);
  EXPECT_EQ(moving.heads[1].position, 121);
}

TEST(FridgeRegionMotionTest,
     ForwardReverseAndFadingHeadsStayWithinTheirRegion) {
  config::Config config;
  config.regions[0].range = 10;
  config.heads[0].position = 9;
  config.lfos[0].min_grain_size = 7;
  config.lfos[0].max_grain_size = 7;
  config.lfos[0].reverse_chance = 1.0f;
  mod::Modulator modulator;
  modulator.Reset(config);
  for (int i = 0; i < 100; ++i) {
    const auto& frame = modulator.TickSample();
    EXPECT_LT(modulator.virtual_config().heads[0].position, 10);
    for (size_t h = 0; h < frame.head_count; ++h) {
      EXPECT_LT(frame.heads[h].position,
                config.regions[frame.heads[h].region].range);
    }
  }
}

TEST(FridgeRegionMemoryTest,
     ResizingPreservesOtherRegionsAndReusesReleasedPages) {
  auto memory = std::make_unique<regions::Memory>();
  std::array<config::Region, kNumRegions> regions;
  for (auto& region : regions) {
    region.range = kRegionPageSize;
  }
  ASSERT_TRUE(memory->SetRegions(regions));
  const auto original = memory->Resolve(1, 42);
  EXPECT_TRUE(original.fresh);
  EXPECT_FALSE(memory->Resolve(1, 42).fresh);
  regions[0].range = 4 * kRegionPageSize;
  ASSERT_TRUE(memory->SetRegions(regions));
  EXPECT_EQ(memory->Resolve(1, 42).index, original.index);
  EXPECT_FALSE(memory->Resolve(1, 42).fresh);
  const auto released = memory->Resolve(0, kRegionPageSize + 17);
  regions[0].range = kRegionPageSize;
  ASSERT_TRUE(memory->SetRegions(regions));
  regions[2].range = 2 * kRegionPageSize;
  ASSERT_TRUE(memory->SetRegions(regions));
  const auto reused = memory->Resolve(2, kRegionPageSize + 17);
  EXPECT_EQ(reused.index, released.index);
  EXPECT_TRUE(reused.fresh);
}

TEST(FridgeRegionMemoryTest, ShrinkingInsidePageDiscardsOnlyTheTruncatedTail) {
  auto memory = std::make_unique<regions::Memory>();
  std::array<config::Region, kNumRegions> regions;
  regions[0].range = 1000;
  ASSERT_TRUE(memory->SetRegions(regions));
  memory->Resolve(0, 200);
  memory->Resolve(0, 800);
  regions[0].range = 500;
  ASSERT_TRUE(memory->SetRegions(regions));
  regions[0].range = 1000;
  ASSERT_TRUE(memory->SetRegions(regions));
  EXPECT_FALSE(memory->Resolve(0, 200).fresh);
  EXPECT_TRUE(memory->Resolve(0, 800).fresh);
}

TEST(FridgeRegionMemoryTest, InvalidResizeKeepsMappings) {
  auto memory = std::make_unique<regions::Memory>();
  std::array<config::Region, kNumRegions> regions;
  ASSERT_TRUE(memory->SetRegions(regions));
  const auto original = memory->Resolve(0, 100);
  regions[0].range = kRegionCapacity;
  EXPECT_FALSE(memory->SetRegions(regions));
  EXPECT_EQ(memory->Resolve(0, 100).index, original.index);
  EXPECT_FALSE(memory->Resolve(0, 100).fresh);
}

namespace {
class FridgeRegionSoundTest : public testing::Test {
 protected:
  std::unique_ptr<sound::Sound> sound = std::make_unique<sound::Sound>();
  std::array<config::Region, kNumRegions> regions;
  mod::Frame frame;

  void SetUp() override {
    for (auto& region : regions) {
      region.range = 2 * kRegionPageSize;
    }
    frame.regions = &regions;
    frame.dry = 0;
    frame.head_count = 1;
  }

  void Write(uint8_t region, size_t position, float value) {
    frame.heads[0] = {
        .position = position, .region = region, .write_amount = 1};
    sound->ProcessSample(frame, StereoSample::OfMono(value));
    frame.head_count = 0;
    for (size_t i = 0; i < kFadeTime; ++i) {
      sound->ProcessSample(frame, StereoSample::Zero());
    }
    frame.head_count = 1;
  }

  float Read(uint8_t region, size_t position) {
    frame.heads[0] = {.position = position, .region = region, .read_amount = 1};
    return sound->ProcessSample(frame, StereoSample::Zero()).left;
  }
};
}  // namespace

TEST_F(FridgeRegionSoundTest,
       RegionsAreIsolatedAndHeadsShareTheirAssignedAudio) {
  Write(0, 10, 0.5f);
  EXPECT_FLOAT_EQ(Read(1, 10), 0);
  EXPECT_FLOAT_EQ(Read(0, 10 + regions[0].range), 0.5f);
  frame.head_count = 2;
  frame.heads[0] = {.position = 10, .region = 0, .read_amount = 1};
  frame.heads[1] = frame.heads[0];
  EXPECT_FLOAT_EQ(sound->ProcessSample(frame, StereoSample::Zero()).left, 1);
}

TEST_F(FridgeRegionSoundTest, GrowingAnotherRegionDoesNotMoveRecordedAudio) {
  Write(1, 10, 0.5f);
  regions[0].range = 10 * kRegionPageSize;
  EXPECT_FLOAT_EQ(Read(1, 10), 0.5f);
}

TEST_F(FridgeRegionSoundTest, RecycledPagesDoNotLeakAudioOrPendingWrites) {
  Write(0, kRegionPageSize + 10, 0.5f);
  // Leave another write fading on the soon-to-be-released page.
  frame.heads[0] = {
      .position = kRegionPageSize + 10, .region = 0, .write_amount = 1};
  sound->ProcessSample(frame, StereoSample::OfMono(0.25f));
  regions[0].range = kRegionPageSize;
  regions[1].range = 3 * kRegionPageSize;
  EXPECT_FLOAT_EQ(Read(1, 2 * kRegionPageSize + 10), 0);
  Write(1, 2 * kRegionPageSize + 10, 0.25f);
  EXPECT_FLOAT_EQ(Read(1, 2 * kRegionPageSize + 10), 0.25f);
}

TEST_F(FridgeRegionSoundTest, ShrinkAndRegrowPreservesPrefixAndClearsTail) {
  Write(0, 10, 0.25f);
  Write(0, 800, 0.5f);
  regions[0].range = 500;
  EXPECT_FLOAT_EQ(Read(0, 10), 0.25f);
  regions[0].range = 1000;
  EXPECT_FLOAT_EQ(Read(0, 10), 0.25f);
  EXPECT_FLOAT_EQ(Read(0, 800), 0);
}

TEST(FridgeRegionMotionTest,
     AllPairsCanFadeWhileRegionsAreResizedAndReassigned) {
  auto sound = std::make_unique<sound::Sound>();
  config::Config config;
  for (auto& region : config.regions) {
    region.range = 4000;
  }
  for (size_t i = 0; i < kNumHeads; ++i) {
    config.heads[i].region = i % kNumRegions;
    config.heads[i].position = 3900;
    config.heads[i].write_amount = 0.01f;
    config.heads[i].read_amount = 0.05f;
    config.heads[i].erase_amount = 0.99f;
    config.lfos[i].min_grain_size = 2;
    config.lfos[i].max_grain_size = 12;
    config.lfos[i].reverse_chance = 0.5f;
    config.lfos[i].teleport_chance = 0.5f;
  }
  mod::Modulator modulator(1234);
  modulator.Reset(config);
  for (size_t sample = 0; sample < 2000; ++sample) {
    if (sample % 100 == 0) {
      const size_t region = sample / 100 % kNumRegions;
      ASSERT_TRUE(config.ResizeRegion(region, 1000 + sample));
      ASSERT_TRUE(config.AssignRegion(sample / 100 % kNumHeads, region));
      modulator.SetConfig(config);
    }
    const auto& frame = modulator.TickSample();
    for (size_t h = 0; h < frame.head_count; ++h) {
      ASSERT_LT(frame.heads[h].position,
                config.regions[frame.heads[h].region].range);
    }
    const auto output = sound->ProcessSample(frame, StereoSample::OfMono(0.1f));
    ASSERT_TRUE(std::isfinite(output.left));
    ASSERT_TRUE(std::isfinite(output.right));
  }
}
