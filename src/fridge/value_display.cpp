#include "value_display.hpp"

#include <cstdint>

#include "libjazz/color.hpp"

namespace fridge::ui::value_display {

color::RGB BlackToWhite::operator()(uint8_t v) const {
  return color::RGB(v, v, v);
}

color::RGB HueWheel::operator()(uint8_t v) const {
  /* 1. scale the value to the interval [start, end] */
  v = static_cast<uint8_t>(
      ((static_cast<float>(v) * (static_cast<float>(end - start))) / 255.) +
      start);

  /* 2. Convert to RGB */
  color::RGB rgb = color::HSV(v, saturation, this->value);

  /* 3. Apply gamma scaling */
  rgb.red = color::gamma_scale(rgb.red, red_gamma);
  rgb.green = color::gamma_scale(rgb.green, green_gamma);
  rgb.blue = color::gamma_scale(rgb.blue, blue_gamma);

  return rgb;
}

color::XYZ CieInterp::operator()(uint8_t value) const {
  float t = static_cast<float>(value) / 255.0f;

  auto lerp = [t](uint8_t a, uint8_t b) -> uint8_t {
    return static_cast<uint8_t>(a + (b - a) * t);
  };

  return color::XYZ(lerp(start.x, end.x), lerp(start.y, end.y),
                    lerp(start.z, end.z));
}

}  // namespace fridge::ui::value_display
