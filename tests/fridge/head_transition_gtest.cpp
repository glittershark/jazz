#include <array>
#include <optional>

#include "fridge.hpp"
#include "gtest/gtest.h"

using fridge::NUM_HEADS;
using fridge::config::Config;
using fridge::state::Direction;
using fridge::transition::HeadMotion;
using fridge::transition::HeadMotionTransition;
using fridge::transition::HeadTransitionMixer;

namespace {

std::array<std::optional<HeadMotionTransition>, NUM_HEADS> NoTransitions() {
  return {};
}

}  // namespace

TEST(FridgeHeadTransitionTest, StaticConfigProducesOneContributionPerHead) {
  Config config;
  config.heads[0].position = 44;
  config.heads[0].read_amount = 0.5f;

  HeadTransitionMixer mixer(4);
  const fridge::transition::Frame& frame =
      mixer.Update(config, NoTransitions());

  EXPECT_EQ(frame.head_count, NUM_HEADS);
  EXPECT_EQ(frame.heads[0].head.position, 44u);
  EXPECT_FLOAT_EQ(frame.heads[0].head.read_amount, 0.5f);
}

TEST(FridgeHeadTransitionTest, TransitionFadesOldOutAndNewIn) {
  Config config;
  config.heads[0].position = 100;
  config.heads[0].read_amount = 1.0f;
  config.heads[0].write_amount = 0.5f;
  config.heads[0].erase_amount = 0.25f;

  std::array<std::optional<HeadMotionTransition>, NUM_HEADS> transitions{};
  transitions[0] = HeadMotionTransition{
      .old_motion =
          HeadMotion{
              .position = 12,
              .direction = Direction::kForwards,
              .speed = 1.0f,
          },
      .new_motion =
          HeadMotion{
              .position = 100,
              .direction = Direction::kBackwards,
              .speed = 1.0f,
          },
      .reversed = true,
  };

  HeadTransitionMixer mixer(4);
  const fridge::transition::Frame& first = mixer.Update(config, transitions);

  ASSERT_EQ(first.head_count, NUM_HEADS);
  EXPECT_EQ(first.heads[0].head.position, 12u);
  EXPECT_FLOAT_EQ(first.heads[0].weight, 1.0f);
  EXPECT_FLOAT_EQ(first.heads[0].head.read_amount, 1.0f);

  const fridge::transition::Frame& second =
      mixer.Update(config, NoTransitions());

  ASSERT_EQ(second.head_count, NUM_HEADS + 1);
  EXPECT_EQ(second.heads[0].head.position, 13u);
  EXPECT_FLOAT_EQ(second.heads[0].weight, 0.75f);
  EXPECT_FLOAT_EQ(second.heads[1].weight, 0.25f);
  EXPECT_FLOAT_EQ(second.heads[0].head.read_amount, 0.75f);
  EXPECT_FLOAT_EQ(second.heads[1].head.read_amount, 0.25f);
  EXPECT_FLOAT_EQ(second.heads[0].head.write_amount, 0.375f);
  EXPECT_FLOAT_EQ(second.heads[1].head.write_amount, 0.125f);
  EXPECT_FLOAT_EQ(second.heads[0].head.erase_amount, 0.4375f);
  EXPECT_FLOAT_EQ(second.heads[1].head.erase_amount, 0.8125f);
}

TEST(FridgeHeadTransitionTest, TransitionEndsAfterFadeTime) {
  Config config;
  config.heads[0].position = 100;

  std::array<std::optional<HeadMotionTransition>, NUM_HEADS> transitions{};
  transitions[0] = HeadMotionTransition{
      .old_motion =
          HeadMotion{
              .position = 12,
              .direction = Direction::kForwards,
              .speed = 1.0f,
          },
      .new_motion =
          HeadMotion{
              .position = 100,
              .direction = Direction::kBackwards,
              .speed = 1.0f,
          },
      .teleported = true,
  };

  HeadTransitionMixer mixer(2);
  mixer.Update(config, transitions);
  mixer.Update(config, NoTransitions());
  const fridge::transition::Frame& finished =
      mixer.Update(config, NoTransitions());

  EXPECT_EQ(finished.head_count, NUM_HEADS);
  EXPECT_EQ(finished.heads[0].head.position, 100u);
}
