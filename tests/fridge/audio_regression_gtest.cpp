#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <type_traits>
#include <vector>

#include "gtest/gtest.h"
#include "sound.hpp"

using namespace fridge;
using jazz::audio::StereoSample;

namespace {
class AudioRegressionTest : public testing::Test {
 protected:
  std::unique_ptr<sound::Sound> sound = std::make_unique<sound::Sound>();
  mod::Frame frame;

  void SetUp() override {
    frame.head_count = 1;
    frame.dry = 0;
  }

  float Tick(float input = 0) {
    const auto output =
        sound->ProcessSample(frame, StereoSample::OfMono(input));
    EXPECT_FLOAT_EQ(output.left, output.right);
    return output.left;
  }

  void Settle() {
    frame.heads[0].write_amount = 0;
    frame.heads[0].erase_amount = 1;
    for (size_t i = 0; i < kFadeTime; ++i) {
      Tick();
    }
    frame.heads[0].read_amount = 1;
  }
};
}  // namespace

TEST_F(AudioRegressionTest, LaterEraseKeepsItsDeadlineAfterInterveningWrite) {
  frame.heads[0].erase_amount = 0.5f;
  Tick();
  frame.heads[0].erase_amount = 1;
  frame.heads[0].write_amount = 1;
  Tick(0.4f);
  frame.heads[0].write_amount = 0;
  frame.heads[0].erase_amount = 0.5f;
  Tick();
  Settle();
  EXPECT_NEAR(Tick(), 0.2f, 1e-6f);
}

TEST_F(AudioRegressionTest, SimultaneousErasesMultiplyBeforeWrites) {
  frame.heads[0].write_amount = 1;
  Tick(0.8f);
  Settle();
  frame.head_count = 2;
  frame.heads[0].erase_amount = 0.5f;
  frame.heads[1].erase_amount = 0.25f;
  frame.heads[1].write_amount = 1;
  Tick(0.2f);
  frame.head_count = 1;
  Settle();
  EXPECT_NEAR(Tick(), 0.3f, 1e-6f);
}

TEST_F(AudioRegressionTest, WriteFadeMatchesEveryIntermediateSample) {
  frame.heads[0].write_amount = 1;
  Tick(0.5f);
  frame.heads[0].write_amount = 0;
  frame.heads[0].read_amount = 1;
  for (size_t age = 1; age <= kFadeTime; ++age) {
    EXPECT_NEAR(Tick(), 0.5f * age / kFadeTime, 1e-6f);
  }
}

TEST_F(AudioRegressionTest, StaggeredEraseFadesMultiplyIndependently) {
  frame.heads[0].write_amount = 1;
  Tick(0.8f);
  Settle();
  frame.heads[0].erase_amount = 0.5f;
  Tick();
  frame.heads[0].erase_amount = 0.25f;
  Tick();
  frame.heads[0].erase_amount = 1;
  const float first_offset = kFadeTime * 0.5f / 0.5f;
  const float second_offset = kFadeTime * 0.25f / 0.75f;
  EXPECT_NEAR(Tick(),
              0.8f * first_offset / (2 + first_offset + 1) * second_offset /
                  (1 + second_offset + 1),
              1e-6f);
}

TEST_F(AudioRegressionTest,
       PannedEraseLeavesOppositeChannelFiniteAndUnchanged) {
  frame.heads[0].write_amount = 1;
  Tick(0.8f);
  Settle();
  frame.heads[0].pan = jazz::audio::Pan::Right(1);
  frame.heads[0].erase_amount = 0.5f;
  sound->ProcessSample(frame, StereoSample::Zero());
  frame.heads[0].pan = jazz::audio::Pan::Center();
  frame.heads[0].erase_amount = 1;
  const auto during = sound->ProcessSample(frame, StereoSample::Zero());
  EXPECT_NEAR(during.left, 0.8f, 1e-6f);
  EXPECT_TRUE(std::isfinite(during.right));
  for (size_t i = 0; i < kFadeTime; ++i) {
    sound->ProcessSample(frame, StereoSample::Zero());
  }
  const auto after = sound->ProcessSample(frame, StereoSample::Zero());
  EXPECT_NEAR(after.left, 0.8f, 1e-6f);
  EXPECT_NEAR(after.right, 0.4f, 1e-6f);
}

TEST_F(AudioRegressionTest, OversizedFrameDoesNotReadPastItsHeadArray) {
  frame.head_count = std::numeric_limits<size_t>::max();
  EXPECT_FLOAT_EQ(Tick(), 0);
}

TEST_F(AudioRegressionTest, InvalidRegionFrameDoesNotTouchTape) {
  std::array<config::Region, kNumRegions> regions{};
  regions[0].range = 0;
  frame.regions = &regions;
  frame.heads[0].write_amount = 1;
  Tick(0.5f);
  frame.regions = nullptr;
  Settle();
  EXPECT_FLOAT_EQ(Tick(), 0);
}

TEST_F(AudioRegressionTest, InvalidHeadRegionIsIgnored) {
  std::array<config::Region, kNumRegions> regions{};
  frame.regions = &regions;
  frame.heads[0].region = 255;
  frame.heads[0].read_amount = 1;
  EXPECT_FLOAT_EQ(Tick(), 0);
}

class AudioClockTest : public AudioRegressionTest,
                       public testing::WithParamInterface<uint32_t> {};

TEST_P(AudioClockTest, WritesAndErasesSurviveClockRollover) {
  sound = std::make_unique<sound::Sound>(GetParam());
  frame.heads[0].write_amount = 1;
  Tick(0.8f);
  Settle();
  EXPECT_NEAR(Tick(), 0.8f, 1e-6f);
  frame.heads[0].erase_amount = 0.5f;
  Tick();
  Settle();
  EXPECT_NEAR(Tick(), 0.4f, 1e-6f);
}

INSTANTIATE_TEST_SUITE_P(Boundaries, AudioClockTest,
                         testing::Values(0u, UINT32_MAX - 12, UINT32_MAX - 5,
                                         UINT32_MAX - 1, UINT32_MAX));
