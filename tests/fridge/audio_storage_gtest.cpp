#include <cmath>
#include <memory>
#include <type_traits>
#include "gtest/gtest.h"
#include "sound.hpp"

using namespace fridge;
using jazz::audio::StereoSample;

TEST(AudioStorageTest, HostUsesTheSeedsFourByteSampleRepresentation) {
  EXPECT_EQ(sizeof(sound::BufferValue), 4);
  EXPECT_LE(sizeof(sound::Sound), 64 * 1024 * 1024);
  EXPECT_FALSE(std::is_copy_constructible_v<sound::BufferValue>);
  EXPECT_FALSE(std::is_copy_constructible_v<sound::Sound>);
}

TEST(AudioStorageTest, NonfiniteSamplesCannotBeMistakenForSlabPointers) {
  for (float value : {NAN, -NAN, INFINITY, -INFINITY}) {
    sound::BufferValue sample(value);
    EXPECT_TRUE(sample.isSample());
    EXPECT_FLOAT_EQ(sample.sample(), 0);
    sample.PushBack(
        {.kind = sound::Update::kWrite, .finished_at = 8, .value = value});
    EXPECT_TRUE(sample.isSampleWithUpdates());
    EXPECT_FLOAT_EQ((*sample.FirstUpdate())->value, 0);
  }
}

TEST(AudioStorageTest, MaximumFrameLoadCanBeDestroyedAndReused) {
  for (int run = 0; run < 8; ++run) {
    auto sound = std::make_unique<sound::Sound>();
    mod::Frame frame;
    frame.head_count = frame.heads.size();
    for (auto& head : frame.heads) {
      head.read_amount = 0.01f;
      head.write_amount = 0.001f;
      head.erase_amount = 0.999f;
    }
    for (size_t sample = 0; sample < 257; ++sample) {
      for (size_t i = 0; i < frame.head_count; ++i) {
        frame.heads[i].position = sample * frame.head_count + i;
      }
      auto output = sound->ProcessSample(frame, StereoSample::OfMono(0.1f));
      ASSERT_TRUE(std::isfinite(output.left));
      ASSERT_TRUE(std::isfinite(output.right));
    }
    // Destruction deliberately occurs with a full window of pending updates.
  }
}
