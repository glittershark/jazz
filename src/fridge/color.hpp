#ifndef COLOR_H_
#define COLOR_H_

#include <cstdint>

namespace fridge {
namespace color {

struct HSV;

struct RGB {
    uint8_t red;
    uint8_t green;
    uint8_t blue;

    RGB(uint8_t r, uint8_t g, uint8_t b) : red(r), green(g), blue(b) {}
    RGB(const HSV &hsv);
};

struct HSV {
    uint8_t hue;
    uint8_t saturation;
    uint8_t value;

    HSV(uint8_t h, uint8_t s, uint8_t v) : hue(h), saturation(s), value(v) {}
    HSV(const RGB &rgb);
};

} // namespace color
} // namespace fridge

#endif // COLOR_H_
