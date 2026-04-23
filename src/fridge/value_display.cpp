#include "value_display.hpp"

#include <cstdint>

#include "color.hpp"

namespace fridge::ui::value_display {

color::RGB HueWheel::operator()(uint8_t v) const {
  /* 1. scale the value to the interval [start, end] */
  v = (uint8_t)(((((float)v) * ((float)(end - start))) / 255.) + start);

  /* 2. Convert to RGB */
  color::RGB rgb = color::HSV(v, saturation, this->value);

  /* 3. Apply gamma scaling */
  rgb.red = color::gamma_scale(rgb.red, red_gamma);
  rgb.green = color::gamma_scale(rgb.green, green_gamma);
  rgb.blue = color::gamma_scale(rgb.blue, blue_gamma);

  return rgb;
}  // namespace fridge::ui::value_display

}  // namespace fridge::ui::value_display
