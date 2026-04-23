#include "color.hpp"
#include "gtest/gtest.h"
#include "rapidcheck/gtest.h"
#include "test_util.hpp"
#include "value_display.hpp"

using namespace fridge;
namespace {

RC_GTEST_PROP(HueWheelTest, NoConfig, (const uint8_t input_value)) {
  ui::value_display::HueWheel hue_wheel;
  color::HSV res = hue_wheel(input_value);
  RC_ASSERT(udist(res.hue, input_value) <= 1);
}

TEST(HueWheelTest, Scaling) {
  ui::value_display::HueWheel hue_wheel{
      .start = 50,
      .end = 100,
  };

  color::HSV res = hue_wheel(0);
  EXPECT_EQ(res.hue, 50);

  res = hue_wheel(255);
  EXPECT_NEAR(res.hue, 100, 1);

  res = hue_wheel(128);
  EXPECT_NEAR(res.hue, 75, 1);
}

}  // namespace
