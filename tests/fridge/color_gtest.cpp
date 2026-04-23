#include "color.hpp"
#include "gtest/gtest.h"
#include "rapidcheck/gtest.h"
#include "test_util.hpp"

using namespace fridge::color;

namespace {

TEST(XYZTest, ConstructFromRGB_Black) {
  RGB black(0, 0, 0);
  XYZ xyz(black);
  EXPECT_EQ(xyz.x, 0);
  EXPECT_EQ(xyz.y, 0);
  EXPECT_EQ(xyz.z, 0);
}

TEST(XYZTest, ConstructFromRGB_White) {
  RGB white(255, 255, 255);
  XYZ xyz(white);
  // White should have high Y (luminance)
  EXPECT_GT(xyz.y, 200);
}

TEST(XYZTest, ConvertToRGB_Black) {
  XYZ black(0, 0, 0);
  RGB rgb = black;
  EXPECT_EQ(rgb.red, 0);
  EXPECT_EQ(rgb.green, 0);
  EXPECT_EQ(rgb.blue, 0);
}

TEST(XYZTest, RoundTrip_Black) {
  RGB original(0, 0, 0);
  XYZ xyz(original);
  RGB result = xyz;
  EXPECT_EQ(result.red, original.red);
  EXPECT_EQ(result.green, original.green);
  EXPECT_EQ(result.blue, original.blue);
}

TEST(XYZTest, RoundTrip_White) {
  RGB original(255, 255, 255);
  XYZ xyz(original);
  RGB result = xyz;
  // Allow small error due to quantization
  EXPECT_NEAR(result.red, original.red, 2);
  EXPECT_NEAR(result.green, original.green, 2);
  EXPECT_NEAR(result.blue, original.blue, 2);
}

TEST(XYZTest, RoundTrip_Red) {
  RGB original(255, 0, 0);
  XYZ xyz(original);
  RGB result = xyz;
  EXPECT_NEAR(result.red, original.red, 2);
  EXPECT_NEAR(result.green, original.green, 2);
  EXPECT_NEAR(result.blue, original.blue, 2);
}

TEST(XYZTest, RoundTrip_Green) {
  RGB original(0, 255, 0);
  XYZ xyz(original);
  RGB result = xyz;
  EXPECT_NEAR(result.red, original.red, 2);
  EXPECT_NEAR(result.green, original.green, 2);
  EXPECT_NEAR(result.blue, original.blue, 2);
}

TEST(XYZTest, RoundTrip_Blue) {
  RGB original(0, 0, 255);
  XYZ xyz(original);
  RGB result = xyz;
  EXPECT_NEAR(result.red, original.red, 2);
  EXPECT_NEAR(result.green, original.green, 2);
  EXPECT_NEAR(result.blue, original.blue, 2);
}

TEST(XYZTest, RoundTrip_BrightColors) {
  // Test bright colors where uint8_t XYZ has sufficient precision.
  // Dark colors lose significant precision due to XYZ quantization
  // (XYZ values compress non-linearly near zero).
  rc::check([](uint8_t r, uint8_t g, uint8_t b) {
    // Offset to test bright colors only (64-255 range)
    r = static_cast<uint8_t>(64 + (r % 192));
    g = static_cast<uint8_t>(64 + (g % 192));
    b = static_cast<uint8_t>(64 + (b % 192));

    RGB original(r, g, b);
    XYZ xyz(original);
    RGB result = xyz;

    // Allow for quantization error (uint8_t has limited precision)
    RC_ASSERT(udiff(result.red, original.red) <= 3);
    RC_ASSERT(udiff(result.green, original.green) <= 3);
    RC_ASSERT(udiff(result.blue, original.blue) <= 3);
  });
}

TEST(XYZTest, Luminance_GreenBrighterThanRed) {
  // Green should have higher luminance than red at same intensity
  RGB red(200, 0, 0);
  RGB green(0, 200, 0);
  XYZ xyz_red(red);
  XYZ xyz_green(green);
  EXPECT_GT(xyz_green.y, xyz_red.y);
}

TEST(XYZTest, Luminance_RedBrighterThanBlue) {
  // Red should have higher luminance than blue at same intensity
  RGB red(200, 0, 0);
  RGB blue(0, 0, 200);
  XYZ xyz_red(red);
  XYZ xyz_blue(blue);
  EXPECT_GT(xyz_red.y, xyz_blue.y);
}

}  // namespace
