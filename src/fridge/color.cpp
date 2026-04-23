#include "color.hpp"

#include <cmath>
#include <cstdint>

namespace fridge {

namespace color {

RGB::RGB(const HSV& hsv) {
  if (hsv.saturation == 0) {
    red = green = blue = hsv.value;
    return;
  }

  uint8_t region = hsv.hue / 43;
  uint8_t remainder = (hsv.hue - (region * 43)) * 6;

  uint8_t p = (hsv.value * (255 - hsv.saturation)) >> 8;
  uint8_t q = (hsv.value * (255 - ((hsv.saturation * remainder) >> 8))) >> 8;
  uint8_t t =
      (hsv.value * (255 - ((hsv.saturation * (255 - remainder)) >> 8))) >> 8;

  switch (region) {
  case 0:
    red = hsv.value;
    green = t;
    blue = p;
    break;
  case 1:
    red = q;
    green = hsv.value;
    blue = p;
    break;
  case 2:
    red = p;
    green = hsv.value;
    blue = t;
    break;
  case 3:
    red = p;
    green = q;
    blue = hsv.value;
    break;
  case 4:
    red = t;
    green = p;
    blue = hsv.value;
    break;
  default:
    red = hsv.value;
    green = p;
    blue = q;
    break;
  }
}

HSV::HSV(const RGB& rgb) {
  uint8_t max = rgb.red > rgb.green ? rgb.red : rgb.green;
  max = max > rgb.blue ? max : rgb.blue;
  uint8_t min = rgb.red < rgb.green ? rgb.red : rgb.green;
  min = min < rgb.blue ? min : rgb.blue;
  uint8_t delta = max - min;

  value = max;
  saturation = (max == 0) ? 0 : (uint8_t)((delta * 255) / max);

  if (delta == 0) {
    hue = 0;
  } else if (max == rgb.red) {
    int16_t h = (int16_t)(rgb.green - rgb.blue) * 43 / delta;
    hue = (h < 0) ? (uint8_t)(h + 256) : (uint8_t)h;
  } else if (max == rgb.green) {
    hue = 85 + (int16_t)(rgb.blue - rgb.red) * 43 / delta;
  } else {
    hue = 171 + (int16_t)(rgb.red - rgb.green) * 43 / delta;
  }
}

uint8_t gamma_scale(uint8_t value, float gamma) {
  return (uint8_t)(255. * powf((((float)value) / 255.), gamma));
}

}  // namespace color

}  // namespace fridge
