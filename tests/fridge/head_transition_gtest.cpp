#include "fridge.hpp"
#include "gtest/gtest.h"
#include "libjazz/units.hpp"

using fridge::kNumHeads;
using fridge::config::Config;
using fridge::config::Target;
using fridge::config::TargetObject;
using fridge::config::TargetParameter;
using fridge::mod::Frame;
using fridge::mod::Modulator;

namespace {

/* Head 0 with distinctive amounts, driven by an LFO that reverses at every
 * grain boundary (every `grain_size` samples). */
Config ReversingHeadConfig(size_t grain_size) {
  Config config;
  config.heads[0].position = 10;
  config.heads[0].read_amount = 1.0f;
  config.heads[0].write_amount = 0.5f;
  config.heads[0].erase_amount = 0.25f;
  config.lfos[0] = fridge::config::LFO{.range = 20,
                                       .max_grain_size = grain_size,
                                       .min_grain_size = grain_size,
                                       .reverse_chance = 1.0f};
  config.lfos[0].targets[0] = Target{.object = TargetObject::kHead,
                                     .parameter = TargetParameter::kPosition,
                                     .object_idx = 0};
  return config;
}

}  // namespace

TEST(FridgeHeadFadeTest, StaticConfigProducesOneContributionPerHead) {
  Config config;
  config.heads[0].position = 44;
  config.heads[0].read_amount = 0.5f;

  Modulator modulator(1234, /*fade_time=*/4);
  modulator.Reset(config);
  const Frame& frame = modulator.TickSample();

  EXPECT_EQ(frame.head_count, kNumHeads);
  EXPECT_EQ(frame.heads[0].position, 44u);
  EXPECT_FLOAT_EQ(frame.heads[0].read_amount, 0.5f);
}

TEST(FridgeHeadFadeTest, ReversalFadesOldMotionOutAndNewIn) {
  Modulator modulator(1234, /*fade_time=*/4);
  modulator.Reset(ReversingHeadConfig(/*grain_size=*/2));

  // Sample 1: LFO mid-grain, head simply tracks it forwards.
  const Frame& tracking = modulator.TickSample();
  ASSERT_EQ(tracking.head_count, kNumHeads);
  EXPECT_EQ(tracking.heads[0].position, 11u);

  // Sample 2: grain boundary reverses the LFO. The fade starts at full
  // weight on the old motion, so the new head (weight 0) is omitted.
  const Frame& boundary = modulator.TickSample();
  ASSERT_EQ(boundary.head_count, kNumHeads);
  EXPECT_EQ(boundary.heads[0].position, 11u);
  EXPECT_FLOAT_EQ(boundary.heads[0].read_amount, 1.0f);

  // Sample 3: the old motion keeps moving forwards (12) at weight 3/4 while
  // the reversed head (11) fades in at weight 1/4.
  const Frame& fading = modulator.TickSample();
  ASSERT_EQ(fading.head_count, kNumHeads + 1);
  EXPECT_EQ(fading.heads[0].position, 12u);
  EXPECT_FLOAT_EQ(fading.heads[0].read_amount, 0.75f);
  EXPECT_FLOAT_EQ(fading.heads[0].write_amount, 0.375f);
  EXPECT_FLOAT_EQ(fading.heads[0].erase_amount, 0.4375f);
  EXPECT_EQ(fading.heads[1].position, 11u);
  EXPECT_FLOAT_EQ(fading.heads[1].read_amount, 0.25f);
  EXPECT_FLOAT_EQ(fading.heads[1].write_amount, 0.125f);
  EXPECT_FLOAT_EQ(fading.heads[1].erase_amount, 0.8125f);
}

TEST(FridgeHeadFadeTest, FadeEndsAfterFadeTime) {
  Modulator modulator(1234, /*fade_time=*/2);
  modulator.Reset(ReversingHeadConfig(/*grain_size=*/2));

  modulator.TickSample();  // tracking
  modulator.TickSample();  // reversal at the grain boundary

  // Stop further reversals so the fade can run out undisturbed.
  Config no_reverse = ReversingHeadConfig(/*grain_size=*/2);
  no_reverse.lfos[0].reverse_chance = 0.0f;
  modulator.SetConfig(no_reverse);

  modulator.TickSample();  // mid-fade
  const Frame& finished = modulator.TickSample();

  EXPECT_EQ(finished.head_count, kNumHeads);
  for (const auto& fade : modulator.fades()) {
    EXPECT_EQ(fade.remaining, 0u);
  }
}
