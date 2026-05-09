#ifndef COLOR_H_
#define COLOR_H_

#ifdef UNIT_TEST
#include <rapidcheck.h>
#endif  // UNIT_TEST

#include <cstdint>
#include <ostream>

namespace fridge {
namespace color {

struct HSV;

struct RGB {
  uint8_t red;
  uint8_t green;
  uint8_t blue;

  RGB(uint8_t r, uint8_t g, uint8_t b) : red(r), green(g), blue(b) {}
  RGB(const HSV& hsv);
  RGB(const RGB&) = default;
};

struct HSV {
  uint8_t hue;
  uint8_t saturation;
  uint8_t value;

  HSV(uint8_t h, uint8_t s, uint8_t v) : hue(h), saturation(s), value(v) {}
  HSV(const RGB& rgb);
  HSV(const HSV&) = default;
};

/* CIE 1931 colorspace */
struct XYZ {
  uint8_t x;
  uint8_t y;
  uint8_t z;

  XYZ(uint8_t x, uint8_t y, uint8_t z) : x(x), y(y), z(z) {}
  XYZ(const RGB& rgb);
  operator RGB() const;
  XYZ(const XYZ&) = default;
  bool operator==(const XYZ& other) const;
};

std::ostream& operator<<(std::ostream& os, const XYZ& xyz);

uint8_t gamma_scale(uint8_t value, float gamma);

}  // namespace color
}  // namespace fridge

#ifdef UNIT_TEST

namespace rc {

using namespace fridge::color;

template <>
struct Arbitrary<RGB> {
  static Gen<RGB> arbitrary() {
    return gen::construct<RGB>(gen::arbitrary<uint8_t>(),
                               gen::arbitrary<uint8_t>(),
                               gen::arbitrary<uint8_t>());
  }
};

template <>
struct Arbitrary<HSV> {
  static Gen<HSV> arbitrary() {
    return gen::construct<HSV>(gen::arbitrary<uint8_t>(),
                               gen::arbitrary<uint8_t>(),
                               gen::arbitrary<uint8_t>());
  }
};

template <>
struct Arbitrary<XYZ> {
  static Gen<XYZ> arbitrary() {
    return gen::construct<XYZ>(gen::arbitrary<uint8_t>(),
                               gen::arbitrary<uint8_t>(),
                               gen::arbitrary<uint8_t>());
  }
};

}  // namespace rc

#endif  // UNIT_TEST

#endif  // COLOR_H_
