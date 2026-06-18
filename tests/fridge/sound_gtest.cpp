#include <cstddef>
#include <memory>
#include <vector>

#include "config.hpp"
#include "constants.hpp"
#include "gtest/gtest.h"
#include "sound.hpp"

using fridge::BUFFER_LEN;
using fridge::FADE_TIME;
using fridge::NUM_HEADS;
using fridge::sound::BufferValue;
using fridge::sound::IndicesToUpdate;
using fridge::sound::Sound;
using fridge::sound::Update;

TEST(FridgeIndicesToUpdateTest, Prepend) {
  IndicesToUpdate* itu = nullptr;
  IndicesToUpdate::Prepend(&itu, 1);

  ASSERT_EQ((*itu).index(), 1);
  ASSERT_EQ((*itu).next(), nullptr);

  for (auto&& itu_ : itu->iter()) {
    ASSERT_EQ((*itu).index(), 1);
  }

  for (auto&& itu_ : itu->drain()) {
    ASSERT_EQ((*itu).index(), 1);
  }
}

TEST(FridgeIndicesToUpdateTest, IterEmpty) {
  IndicesToUpdate* itu = nullptr;
  // NOLINTNEXTLINE(clang-analyzer-core.CallAndMessage)
  for (auto&& itu_ : itu->iter()) {
    ASSERT_FALSE(true);
  }
}

TEST(FridgeBufferValueTest, DefaultConstructor) {
  BufferValue v;
  EXPECT_EQ(v.sample(), 0.0f);
}

TEST(FridgeBufferValueTest, Sample) {
  BufferValue v1(0.7f);
  EXPECT_NEAR(v1.sample(), 0.7f, 0.0001f);

  BufferValue v2(-0.7f);
  EXPECT_NEAR(v2.sample(), -0.7f, 0.0001f);
}

TEST(FridgeBufferValueTest, SampleWithUpdates) {
  BufferValue v(1.0f);
  v.PushBack({.kind = Update::Kind::kErase, .finished_at = 1, .value = 1.0f});
  EXPECT_EQ(v.sample(), 1.0f);
  EXPECT_TRUE(v.isSampleWithUpdates());
  auto update = v.FirstUpdate();
  EXPECT_NE(update, nullptr);
  EXPECT_NE(*update, nullptr);
  EXPECT_EQ((*update)->kind, Update::Kind::kErase);
}

class FridgeSoundTest : public ::testing::Test {
 protected:
  Sound sound;
  fridge::config::Config config;

  void SetUp() override {
    // Default config: one head at position 0 with full read/write
    config.heads[0].position = 0;
    config.heads[0].read_amount = 1.0f;
    config.heads[0].write_amount = 1.0f;
    config.heads[0].erase_amount = 1.0f;
    config.dry = 0.0f;
    config.wet = 1.0f;

    // Disable other heads
    for (size_t i = 1; i < NUM_HEADS; ++i) {
      config.heads[i].read_amount = 0.0f;
      config.heads[i].write_amount = 0.0f;
      config.heads[i].erase_amount = 1.0f;
    }
  }
};

TEST_F(FridgeSoundTest, InitialReadReturnsZero) {
  // Read-only config at position 0
  config.heads[0].write_amount = 0.0f;

  float output = sound.ProcessSample(config, 0.0f);
  EXPECT_EQ(output, 0.0f);
}

TEST_F(FridgeSoundTest, ReadManyTimesReturnsZero) {
  config.heads[0].write_amount = 0.0f;

  for (int i = 0; i < 1000; ++i) {
    EXPECT_EQ(sound.ProcessSample(config, 0.0f), 0.0f);
  }
}

TEST_F(FridgeSoundTest, WriteAndReadAfterFadeTime) {
  // Write a sample
  config.heads[0].read_amount = 0.0f;
  sound.ProcessSample(config, 0.7f);

  // Advance past fade time with no writes
  config.heads[0].write_amount = 0.0f;
  for (size_t i = 0; i < FADE_TIME; ++i) {
    sound.ProcessSample(config, 0.0f);
  }

  // Now read back
  config.heads[0].read_amount = 1.0f;
  float output = sound.ProcessSample(config, 0.0f);
  EXPECT_NEAR(output, 0.7f, 0.0001f);
}

TEST_F(FridgeSoundTest, WriteNegativeSample) {
  // Write a negative sample
  config.heads[0].read_amount = 0.0f;
  sound.ProcessSample(config, -0.5f);

  // Advance past fade time
  config.heads[0].write_amount = 0.0f;
  for (size_t i = 0; i < FADE_TIME; ++i) {
    sound.ProcessSample(config, 0.0f);
  }

  // Read back
  config.heads[0].read_amount = 1.0f;
  float output = sound.ProcessSample(config, 0.0f);
  EXPECT_NEAR(output, -0.5f, 0.0001f);
}

TEST_F(FridgeSoundTest, WriteToManyPositions) {
  // Write to multiple positions by moving the head
  config.heads[0].read_amount = 0.0f;

  for (size_t i = 0; i < BUFFER_LEN + 10; ++i) {
    config.heads[0].position = i % BUFFER_LEN;
    sound.ProcessSample(config, 0.5f);
  }
}

TEST_F(FridgeSoundTest, DryWetMix) {
  // Test that dry/wet mix works correctly
  config.heads[0].write_amount = 0.0f;
  config.heads[0].read_amount = 0.0f;
  config.dry = 0.5f;
  config.wet = 0.5f;

  float output = sound.ProcessSample(config, 1.0f);
  EXPECT_NEAR(output, 0.5f, 0.0001f);  // Only dry signal, wet is 0
}

TEST_F(FridgeSoundTest, MultipleHeadsRead) {
  // Write to position 0
  config.heads[0].read_amount = 0.0f;
  sound.ProcessSample(config, 0.4f);

  // Write to position 100
  config.heads[0].position = 100;
  sound.ProcessSample(config, 0.3f);

  // Advance past fade time
  config.heads[0].write_amount = 0.0f;
  for (size_t i = 0; i < FADE_TIME; ++i) {
    sound.ProcessSample(config, 0.0f);
  }

  // Read from both positions with two heads
  config.heads[0].position = 0;
  config.heads[0].read_amount = 1.0f;
  config.heads[1].position = 100;
  config.heads[1].read_amount = 1.0f;
  config.dry = 0.0f;
  config.wet = 1.0f;

  float output = sound.ProcessSample(config, 0.0f);
  EXPECT_NEAR(output, 0.7f, 0.0001f);  // 0.4 + 0.3
}

TEST_F(FridgeSoundTest, EraseReducesSignal) {
  // Write to position 0
  config.heads[0].read_amount = 0.0f;
  sound.ProcessSample(config, 1.0f);

  // Advance past fade time
  config.heads[0].write_amount = 0.0f;
  for (size_t i = 0; i < FADE_TIME; ++i) {
    sound.ProcessSample(config, 0.0f);
  }

  // Now erase (erase_amount multiplies the sample, so 0.5 keeps half)
  config.heads[0].position = 0;
  config.heads[0].erase_amount = 0.5f;
  sound.ProcessSample(config, 0.0f);

  // Advance past fade time for erase
  config.heads[0].erase_amount = 1.0f;
  for (size_t i = 0; i < FADE_TIME; ++i) {
    sound.ProcessSample(config, 0.0f);
  }

  // Read - should be reduced to ~0.5
  config.heads[0].read_amount = 1.0f;
  float output = sound.ProcessSample(config, 0.0f);
  EXPECT_NEAR(output, 0.5f, 0.01f);
}

// Verify that Sound's destructor properly frees all slab entries, by allocating
// a bunch of Sound instances and freeing them
TEST_F(FridgeSoundTest, DestructorCleansUpSlabs) {
  fridge::config::Config config;

  // Collect reference outputs from the first Sound instance.
  std::vector<float> reference;
  reference.reserve(FADE_TIME + 1);
  {
    auto sound = std::make_unique<Sound>();
    for (size_t i = 0; i <= FADE_TIME; ++i) {
      reference.push_back(sound->ProcessSample(config, 1.0f));
    }
  }

  // Each subsequent instance must produce identical output. 20 iterations is
  // well past the ~6 needed to overflow either slab without the fix.
  for (int j = 0; j < 20; ++j) {
    auto sound = std::make_unique<Sound>();
    for (size_t i = 0; i <= FADE_TIME; ++i) {
      ASSERT_EQ(sound->ProcessSample(config, 1.0f), reference[i])
          << "output mismatch at iteration " << j << ", sample " << i;
    }
  }
}

TEST_F(FridgeSoundTest, ZeroEraseAmountClearsSignal) {
  config.heads[0].read_amount = 0.0f;
  sound.ProcessSample(config, 1.0f);

  config.heads[0].write_amount = 0.0f;
  for (size_t i = 0; i < FADE_TIME; ++i) {
    sound.ProcessSample(config, 0.0f);
  }

  config.heads[0].position = 0;
  config.heads[0].erase_amount = 0.0f;
  sound.ProcessSample(config, 0.0f);

  config.heads[0].erase_amount = 1.0f;
  for (size_t i = 0; i < FADE_TIME; ++i) {
    sound.ProcessSample(config, 0.0f);
  }

  config.heads[0].read_amount = 1.0f;
  float output = sound.ProcessSample(config, 0.0f);
  EXPECT_NEAR(output, 0.0f, 0.01f);
}
