#include <cstddef>

#include "config.hpp"
#include "gtest/gtest.h"

namespace {

using fridge::config::Head;
using fridge::config::LFO;
using fridge::config::Target;
using fridge::config::TargetObject;
using fridge::config::TargetParameter;
using jazz::audio::Pan;

TEST(EraseAmountTest, an_inert_head_erases_nothing_however_it_is_panned) {
  Head head;
  ASSERT_FLOAT_EQ(head.erase_amount, 1.0f);

  for (float p : {-1.0f, -0.5f, 0.0f, 0.5f, 1.0f}) {
    head.pan = Pan(p);
    EXPECT_FLOAT_EQ(head.EraseAmount().left, 1.0f) << "pan = " << p;
    EXPECT_FLOAT_EQ(head.EraseAmount().right, 1.0f) << "pan = " << p;
  }
}

TEST(EraseAmountTest, a_centered_head_erases_both_channels_equally) {
  Head head;
  head.erase_amount = 0.25f;
  head.pan = Pan::Center();

  EXPECT_FLOAT_EQ(head.EraseAmount().left, 0.25f);
  EXPECT_FLOAT_EQ(head.EraseAmount().right, 0.25f);
}

TEST(EraseAmountTest, panning_attenuates_the_erase_on_the_far_channel) {
  Head head;
  head.erase_amount = 0.0f;

  head.pan = Pan::Right(1.0f);
  EXPECT_FLOAT_EQ(head.EraseAmount().left, 1.0f);
  EXPECT_FLOAT_EQ(head.EraseAmount().right, 0.0f);

  head.pan = Pan::Left(1.0f);
  EXPECT_FLOAT_EQ(head.EraseAmount().left, 0.0f);
  EXPECT_FLOAT_EQ(head.EraseAmount().right, 1.0f);

  // Half right: the left channel erases at half strength.
  head.pan = Pan::Right(0.5f);
  EXPECT_FLOAT_EQ(head.EraseAmount().left, 0.5f);
  EXPECT_FLOAT_EQ(head.EraseAmount().right, 0.0f);
}

void check_invariant(const LFO& lfo) {
  bool found_null = false;
  for (auto& target : lfo.targets) {
    if (found_null) {
      EXPECT_EQ(target, std::nullopt);
    }
    if (!target.has_value()) {
      found_null = true;
    }
  }
}

static LFO lfo_with_targets() {
  LFO lfo;
  lfo.targets[0] = Target{
      .object = TargetObject::kHead,
      .parameter = TargetParameter::kPosition,
      .object_idx = 0,
  };
  lfo.targets[1] = Target{
      .object = TargetObject::kHead,
      .parameter = TargetParameter::kReadAmount,
      .object_idx = 1,
  };
  lfo.targets[2] = Target{
      .object = TargetObject::kHead,
      .parameter = TargetParameter::kPosition,
      .object_idx = 3,
  };

  check_invariant(lfo);

  return lfo;
};

TEST(ToggleTargetTest, removes_target_at_end_of_array) {
  const auto prev = lfo_with_targets();
  auto lfo = prev;
  auto result = lfo.ToggleTarget(Target{
      .object = TargetObject::kHead,
      .parameter = TargetParameter::kPosition,
      .object_idx = 3,
  });
  check_invariant(lfo);

  EXPECT_EQ(result, fridge::config::ToggleResult::kToggledOff);
  EXPECT_EQ(lfo.targets[0], prev.targets[0]);
  EXPECT_EQ(lfo.targets[1], prev.targets[1]);
  for (size_t i = 2; i < lfo.targets.size(); ++i) {
    EXPECT_EQ(lfo.targets[i], std::nullopt);
  }
}

TEST(ToggleTargetTest, removes_target_not_at_end_of_array) {
  const auto prev = lfo_with_targets();
  auto lfo = prev;
  auto result = lfo.ToggleTarget(Target{
      .object = TargetObject::kHead,
      .parameter = TargetParameter::kReadAmount,
      .object_idx = 1,
  });
  check_invariant(lfo);

  EXPECT_EQ(result, fridge::config::ToggleResult::kToggledOff);
  EXPECT_EQ(lfo.targets[0], prev.targets[0]);
  EXPECT_EQ(lfo.targets[1], prev.targets[2]);
  for (size_t i = 2; i < lfo.targets.size(); ++i) {
    EXPECT_EQ(lfo.targets[i], std::nullopt);
  }
}

TEST(ToggleTargetTest, adds_target_when_not_full) {
  const auto prev = lfo_with_targets();
  auto lfo = prev;

  Target new_target{
      .object = TargetObject::kLFO,
      .parameter = TargetParameter::kMaxGrainSize,
      .object_idx = 4,
  };
  auto result = lfo.ToggleTarget(new_target);
  check_invariant(lfo);

  EXPECT_EQ(result, fridge::config::ToggleResult::kToggledOn);
  for (size_t i = 0; i <= 2; ++i) {
    EXPECT_EQ(lfo.targets[i], prev.targets[i]);
  }
  EXPECT_EQ(lfo.targets[3], new_target);
}

static LFO lfo_with_full_targets() {
  LFO lfo;
  for (size_t i = 0; i < lfo.targets.size(); ++i) {
    lfo.targets[i] = Target{
        .object = TargetObject::kHead,
        .parameter = TargetParameter::kPosition,
        .object_idx = static_cast<uint8_t>(i),
    };
  }

  check_invariant(lfo);

  return lfo;
}

TEST(ToggleTargetTest, removes_target_from_a_full_array) {
  const auto prev = lfo_with_full_targets();
  auto lfo = prev;
  auto result = lfo.ToggleTarget(*prev.targets[0]);
  check_invariant(lfo);

  EXPECT_EQ(result, fridge::config::ToggleResult::kToggledOff);

  // Everything shuffles down one, and the vacated tail is cleared rather than
  // left holding a second copy of what used to be last.
  for (size_t i = 0; i + 1 < prev.targets.size(); ++i) {
    EXPECT_EQ(lfo.targets[i], prev.targets[i + 1]);
  }
  EXPECT_EQ(lfo.targets.back(), std::nullopt);
}

TEST(ToggleTargetTest, removing_the_last_target_of_a_full_array_removes_it) {
  const auto prev = lfo_with_full_targets();
  auto lfo = prev;
  auto result = lfo.ToggleTarget(*prev.targets.back());
  check_invariant(lfo);

  EXPECT_EQ(result, fridge::config::ToggleResult::kToggledOff);
  for (const auto& target : lfo.targets) {
    EXPECT_NE(target, prev.targets.back());
  }
}

TEST(ToggleTargetTest, does_nothing_when_full) {
  auto prev = lfo_with_targets();

  for (size_t i = 3; i < prev.targets.size(); ++i) {
    prev.targets[i] = {
        .object = TargetObject::kHead,
        .parameter = TargetParameter::kPosition,
        .object_idx = static_cast<uint8_t>(i),
    };
  }

  auto lfo = prev;
  auto result = lfo.ToggleTarget({
      .object = TargetObject::kLFO,
      .parameter = TargetParameter::kMaxGrainSize,
      .object_idx = 4,
  });
  check_invariant(lfo);

  EXPECT_EQ(result, fridge::config::ToggleResult::kNothingHappened);
  for (size_t i = 0; i < lfo.targets.size(); ++i) {
    EXPECT_EQ(lfo.targets[i], prev.targets[i]);
  }
}

}  // namespace
