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

TEST(CieInterpTest, Interpolation) {
  ui::value_display::CieInterp interp{
      .start = color::XYZ(0, 100, 200),
      .end = color::XYZ(100, 200, 50),
  };

  // At 0, should return start
  color::XYZ res = interp(0);
  EXPECT_EQ(res.x, 0);
  EXPECT_EQ(res.y, 100);
  EXPECT_EQ(res.z, 200);

  // At 255, should return end
  res = interp(255);
  EXPECT_EQ(res.x, 100);
  EXPECT_EQ(res.y, 200);
  EXPECT_EQ(res.z, 50);

  // At 128, should return midpoint (approximately)
  res = interp(128);
  EXPECT_NEAR(res.x, 50, 1);
  EXPECT_NEAR(res.y, 150, 1);
  EXPECT_NEAR(res.z, 125, 1);
}

}  // namespace
