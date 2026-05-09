#include <cstdint>

#include "../test_util.hpp"
#include "gtest/gtest.h"
#include "libjazz/color.hpp"
#include "rapidcheck/gtest.h"
#include "value_display.hpp"

using namespace jazz;
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

TEST(MultiSegmentCieInterpTest, MultiSegmentInterpolation) {
  ui::value_display::MultiSegmentCieInterp<2> interp{
      .start = color::XYZ(0, 0, 255),
      .midpoints = {{{.color = color::XYZ(150, 0, 255), .point = 150},
                     {.color = color::XYZ(200, 0, 255), .point = 200}}},
      .end = color::XYZ(255, 0, 255),
  };

  color::XYZ res = interp(0);
  EXPECT_EQ(res, color::XYZ(0, 0, 255));

  res = interp(100);
  EXPECT_EQ(res, color::XYZ(58, 0, 255));

  res = interp(160);
  EXPECT_EQ(res, color::XYZ(181, 0, 255));

  res = interp(220);
  EXPECT_EQ(res, color::XYZ(213, 0, 255));

  res = interp(255);
  EXPECT_EQ(res, color::XYZ(255, 0, 255));
}

RC_GTEST_PROP(MultiSegmentCieInterpTest, NoSegmentsIsEquivalentToInterp,
              (const color::XYZ start, const color::XYZ end,
               const uint8_t value)) {
  ui::value_display::MultiSegmentCieInterp<0> interp{.start = start,
                                                     .end = end};
  ui::value_display::CieInterp oracle{
      .start = start,
      .end = end,
  };

  color::XYZ res = interp(value);
  color::XYZ expected = oracle(value);
  EXPECT_EQ(res, expected);
}

}  // namespace
