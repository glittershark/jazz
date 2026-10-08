#include <cmath>
#include <type_traits>

#include "gtest/gtest.h"
#include "mod.hpp"

using namespace fridge;
using jazz::units::Samples;

TEST(ModulatorPropertyTest, PairedFastPathMatchesEquivalentGeneralRouting) {
  config::Config paired;
  for (size_t i = 0; i < kNumHeads; ++i) {
    paired.heads[i].position = i * 100;
    paired.heads[i].read_amount = i / 10.0f;
    paired.heads[i].write_amount = i / 20.0f;
    paired.heads[i].erase_amount = 0.75f;
    paired.lfos[i].range = paired.regions[0].range;
    paired.lfos[i].min_grain_size = 4096;
    paired.lfos[i].max_grain_size = 4096;
    paired.lfos[i].targets[0] =
        config::Target{.object = config::TargetObject::kHead,
                       .parameter = config::TargetParameter::kPosition,
                       .object_idx = static_cast<uint8_t>(i)};
  }
  auto assigned = paired;
  assigned.routing = config::Routing::kAssignable;
  mod::Modulator fast(1234), general(1234);
  fast.Reset(paired);
  general.Reset(assigned);
  for (int tick = 0; tick < 1024; ++tick) {
    const auto& actual = fast.TickSample();
    const auto& expected = general.TickSample();
    ASSERT_EQ(actual.head_count, expected.head_count);
    for (size_t h = 0; h < actual.head_count; ++h) {
      EXPECT_EQ(actual.heads[h], expected.heads[h]);
    }
  }
}

TEST(ModulatorPropertyTest, PairedGainAndPanEditsRemainLiveWithoutGainRoutes) {
  config::Config config;
  mod::Modulator modulator;
  modulator.Reset(config);
  for (size_t edit = 0; edit < 20; ++edit) {
    auto& head = config.heads[edit % kNumHeads];
    head.write_amount = edit / 20.0f;
    head.read_amount = edit / 40.0f;
    head.erase_amount = 1 - edit / 40.0f;
    head.feedback.amount = edit / 50.0f;
    head.pan = jazz::audio::Pan(edit / 20.0f - 0.5f);
    modulator.SetConfig(config);
    for (size_t tick = 0; tick < 17; ++tick) {
      const auto& frame = modulator.TickSample();
      ASSERT_EQ(frame.head_count, kNumHeads);
      for (size_t h = 0; h < kNumHeads; ++h) {
        auto actual = frame.heads[h];
        actual.position = config.heads[h].position;
        EXPECT_EQ(actual, config.heads[h]);
      }
    }
  }
}

TEST(ModulatorPropertyTest, InvalidRoutesAreIgnoredWithoutCorruptingValidOnes) {
  config::Config config;
  config.routing = config::Routing::kAssignable;
  config.lfos[0].range = 100;
  config.lfos[0].targets[0] =
      config::Target{.object = config::TargetObject::kHead,
                     .parameter = config::TargetParameter::kPosition,
                     .object_idx = 255};
  config.lfos[0].targets[1] =
      config::Target{.object = static_cast<config::TargetObject>(255),
                     .parameter = config::TargetParameter::kPosition};
  config.lfos[0].targets[2] =
      config::Target{.object = config::TargetObject::kHead,
                     .parameter = config::TargetParameter::kDry};
  config.lfos[0].targets[3] =
      config::Target{.object = config::TargetObject::kHead,
                     .parameter = config::TargetParameter::kPosition,
                     .object_idx = 1};
  mod::Modulator modulator;
  const auto& result = modulator.Update(config, Samples(10u));
  EXPECT_EQ(result.heads[0].position, 0);
  EXPECT_EQ(result.heads[1].position, 10);
  EXPECT_FLOAT_EQ(result.dry, 1);
}

TEST(ModulatorPropertyTest, InstancesKeepIndependentFramesAndRandomEvolution) {
  config::Config config;
  for (auto& lfo : config.lfos) {
    lfo.reverse_chance = 0.5f;
    lfo.teleport_chance = 0.7f;
  }
  mod::Modulator a(19), b(19), disturbance(77);
  a.Reset(config);
  b.Reset(config);
  disturbance.Reset(config);
  for (size_t i = 0; i < 1000; ++i) {
    const auto& first = a.TickSample();
    disturbance.TickSample();
    const auto& second = b.TickSample();
    ASSERT_EQ(first.head_count, second.head_count);
    for (size_t h = 0; h < first.head_count; ++h) {
      EXPECT_EQ(first.heads[h], second.heads[h]);
    }
  }
}
