#include "granular.hpp"
#include "gtest/gtest.h"
#include <cstddef>

TEST(IndicesToUpdateTest, Prepend) {
  IndicesToUpdate *itu = nullptr;
  IndicesToUpdate::Prepend(&itu, 1);

  ASSERT_EQ((*itu).index(), 1);
  ASSERT_EQ((*itu).next(), nullptr);

  for (auto &&itu_ : itu->iter()) {
    ASSERT_EQ((*itu).index(), 1);
  }

  for (auto &&itu_ : itu->drain()) {
    ASSERT_EQ((*itu).index(), 1);
  }
}

TEST(IndicesToUpdateTest, IterEmpty) {
  IndicesToUpdate *itu = nullptr;
  for (auto &&itu_ : itu->iter()) {
    ASSERT_FALSE(true);
  }
}

class GranularTest : public ::testing::Test {
protected:
  Granular granular;

  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(GranularTest, WriteOne) { granular.Write(0, 0, 0.5); }
TEST_F(GranularTest, WriteMany) {
  for (auto clock_time = 0; clock_time < BUFFER_LEN + 10; ++clock_time) {
    granular.PreHousekeeping(clock_time);
    auto index = clock_time % BUFFER_LEN;
    granular.Write(index, clock_time, 0.5);
  };
}
