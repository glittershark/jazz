#include <cstddef>
#include <memory>

#include "granular.hpp"
#include "gtest/gtest.h"

TEST(IndicesToUpdateTest, Prepend) {
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

TEST(IndicesToUpdateTest, IterEmpty) {
  IndicesToUpdate* itu = nullptr;
  for (auto&& itu_ : itu->iter()) {
    ASSERT_FALSE(true);
  }
}

TEST(BufferValueTest, DefaultConstructor) {
  BufferValue v;
  EXPECT_EQ(v.sample(), 0.0f);
}

TEST(BufferValueTest, Sample) {
  BufferValue v1(0.7f);
  EXPECT_NEAR(v1.sample(), 0.7f, 0.0001f);

  BufferValue v2(-0.7f);
  EXPECT_NEAR(v2.sample(), -0.7f, 0.0001f);
}

TEST(BufferValueTest, SampleWithUpdates) {
  BufferValue v(1.0f);
  v.PushBack({.kind = Update::Kind::kErase,
              .finished_at = 1,
              .value = 1.0f,
              .samples = 1});
  EXPECT_EQ(v.sample(), 1.0f);
  EXPECT_TRUE(v.isSampleWithUpdates());
  auto update = v.FirstUpdate();
  EXPECT_NE(update, nullptr);
  EXPECT_NE(*update, nullptr);
  EXPECT_EQ((*update)->kind, Update::Kind::kErase);
}

class GranularTest : public ::testing::Test {
 protected:
  std::unique_ptr<Granular> granular = nullptr;

  void SetUp() override { granular = std::make_unique<Granular>(); }
  void TearDown() override {}
};

TEST_F(GranularTest, BufferZeroed) {
  for (int i = 0; i < BUFFER_LEN; i++) {
    EXPECT_EQ(granular->BUFFER[i].sample(), 0.0f);
  }
}

TEST_F(GranularTest, WriteOne) {
  granular->Write(0, 0, 0.5);
}

TEST_F(GranularTest, JustRead) {
  EXPECT_NEAR(granular->Read(0, 0), 0.0, 0.0001);
}

TEST_F(GranularTest, JustReadManyTimes) {
  for (auto clock_time = 0; clock_time < BUFFER_LEN * 10; ++clock_time) {
    EXPECT_EQ(granular->Read(0, 0), 0.0f);
  }
}

TEST_F(GranularTest, WriteAndRead) {
  granular->Write(0, 0, 0.7, 1);
  EXPECT_NEAR(granular->Read(0, 0), 0.7, 0.0001);
}

TEST_F(GranularTest, WriteNegative) {
  granular->Write(0, 0, -0.5, 1);
  EXPECT_NEAR(granular->Read(0, 0), -0.5, 0.0001);
}

TEST_F(GranularTest, WriteMany) {
  for (auto clock_time = 0; clock_time < BUFFER_LEN + 10; ++clock_time) {
    granular->PreHousekeeping(clock_time);
    auto index = clock_time % BUFFER_LEN;
    granular->Write(index, clock_time, 0.5);
  };
}

TEST_F(GranularTest, WriteVeryMany) {
  for (auto clock_time = 0; clock_time < BUFFER_LEN * 10; ++clock_time) {
    granular->PreHousekeeping(clock_time);
    auto index = clock_time % BUFFER_LEN;
    // Cycle through all problematic values to test they all work
    size_t samples = (clock_time % 64) + 1;  // Test values 1-64
    granular->Write(index, clock_time, 0.5, samples);
  };
}
