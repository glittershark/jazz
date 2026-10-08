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
  EXPECT_NEAR(res.x, 100, 1);
  EXPECT_EQ(res.y, 0);
  EXPECT_EQ(res.z, 255);

  res = interp(160);
  EXPECT_NEAR(res.x, 160, 1);

  res = interp(220);
  EXPECT_NEAR(res.x, 220, 1);

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

TEST(MultiSegmentCieInterpTest, AllInputsFollowTheSameLinearRampAcrossSegments) {
  ui::value_display::MultiSegmentCieInterp<3> interp{
      .start = color::XYZ(0, 255, 42),
      .midpoints = {{{.color = color::XYZ(31, 224, 42), .point = 31},
                     {.color = color::XYZ(150, 105, 42), .point = 150},
                     {.color = color::XYZ(200, 55, 42), .point = 200}}},
      .end = color::XYZ(255, 0, 42)};
  for (int value = 0; value < 256; ++value) {
    const auto result = interp(value);
    EXPECT_NEAR(result.x, value, 1) << value;
    EXPECT_NEAR(result.y, 255 - value, 1) << value;
    EXPECT_EQ(result.z, 42) << value;
  }
}

TEST(MultiSegmentCieInterpTest, ExactBoundariesReturnTheirControlColors) {
  ui::value_display::MultiSegmentCieInterp<2> interp{
      .start = color::XYZ(1, 2, 3),
      .midpoints = {{{.color = color::XYZ(40, 50, 60), .point = 100},
                     {.color = color::XYZ(70, 80, 90), .point = 200}}},
      .end = color::XYZ(100, 110, 120)};
  EXPECT_EQ(interp(0), interp.start);
  EXPECT_EQ(interp(100), interp.midpoints[0].color);
  EXPECT_EQ(interp(200), interp.midpoints[1].color);
  EXPECT_EQ(interp(255), interp.end);
}
