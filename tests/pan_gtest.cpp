#include <gtest/gtest.h>
#include <rapidcheck/gtest.h>

#include "libjazz/pan.hpp"

TEST(PanTest, FullLeft) {
  ASSERT_EQ(jazz::audio::Pan(-1.0f).channels(),
            (jazz::audio::Pan::Channels{.left = 1.0f, .right = 0.0f}));
}

TEST(PanTest, FullRight) {
  ASSERT_EQ(jazz::audio::Pan(1.0f).channels(),
            (jazz::audio::Pan::Channels{.left = 0.0f, .right = 1.0f}));
}

TEST(PanTest, Center) {
  ASSERT_EQ(jazz::audio::Pan::Center().channels(),
            (jazz::audio::Pan::Channels{.left = 0.5f, .right = 0.5f}));
}

TEST(PanTest, LeftConstructor) {
  ASSERT_EQ(jazz::audio::Pan::Left(0.8), jazz::audio::Pan(-0.8));
}

TEST(PanTest, RightConstructor) {
  ASSERT_EQ(jazz::audio::Pan::Right(0.8), jazz::audio::Pan(0.8));
}

RC_GTEST_PROP(PanTest, PanChannelsRoundTrip, (const jazz::audio::Pan pan)) {
  auto channels = pan.channels();
  ASSERT_TRUE(channels.valid());
  auto round_tripped = channels.pan();
  ASSERT_NEAR(round_tripped.pan(), pan.pan(), 0.001f);
}
