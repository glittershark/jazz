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

XYZ::XYZ(const RGB& rgb) {
  // D65 reference white XYZ values (for normalization)
  constexpr float Xn = 0.95047f;
  constexpr float Yn = 1.00000f;
  constexpr float Zn = 1.08883f;

  // Linearize sRGB values (remove gamma)
  auto linearize = [](uint8_t c) -> float {
    float v = c / 255.0f;
    return (v <= 0.04045f) ? v / 12.92f : powf((v + 0.055f) / 1.055f, 2.4f);
  };

  float r = linearize(rgb.red);
  float g = linearize(rgb.green);
  float b = linearize(rgb.blue);

  // sRGB to XYZ matrix (D65 illuminant)
  float fx = 0.4124564f * r + 0.3575761f * g + 0.1804375f * b;
  float fy = 0.2126729f * r + 0.7151522f * g + 0.0721750f * b;
  float fz = 0.0193339f * r + 0.1191920f * g + 0.9503041f * b;

  // Normalize by reference white and scale to 0-255
  x = static_cast<uint8_t>(fminf(fx / Xn * 255.0f, 255.0f));
  y = static_cast<uint8_t>(fminf(fy / Yn * 255.0f, 255.0f));
  z = static_cast<uint8_t>(fminf(fz / Zn * 255.0f, 255.0f));
}

XYZ::operator RGB() const {
  // D65 reference white XYZ values (for denormalization)
  constexpr float Xn = 0.95047f;
  constexpr float Yn = 1.00000f;
  constexpr float Zn = 1.08883f;

  // Convert from 0-255 to normalized XYZ
  float fx = (x / 255.0f) * Xn;
  float fy = (y / 255.0f) * Yn;
  float fz = (z / 255.0f) * Zn;

  // XYZ to linear sRGB matrix (D65 illuminant)
  float r = 3.2404542f * fx - 1.5371385f * fy - 0.4985314f * fz;
  float g = -0.9692660f * fx + 1.8760108f * fy + 0.0415560f * fz;
  float b = 0.0556434f * fx - 0.2040259f * fy + 1.0572252f * fz;

  // Apply sRGB gamma and clamp
  auto gamma_correct = [](float c) -> uint8_t {
    c = fmaxf(0.0f, fminf(1.0f, c));
    float v =
        (c <= 0.0031308f) ? 12.92f * c : 1.055f * powf(c, 1.0f / 2.4f) - 0.055f;
    return static_cast<uint8_t>(v * 255.0f);
  };

  return RGB(gamma_correct(r), gamma_correct(g), gamma_correct(b));
}

bool XYZ::operator==(const XYZ& other) const {
  return other.x == x && other.y == y && other.z == z;
}

std::ostream& operator<<(std::ostream& os, const XYZ& xyz) {
  os << "XYZ{ .x = " << static_cast<uint32_t>(xyz.x)
     << ", .y = " << static_cast<uint32_t>(xyz.y)
     << ", .z = " << static_cast<uint32_t>(xyz.z) << " }";
  return os;
}

uint8_t gamma_scale(uint8_t value, float gamma) {
  // See https://en.wikipedia.org/wiki/Gamma_correction
  float res = 255. * powf(static_cast<float>(value) / 255., gamma);
  return static_cast<uint8_t>(res);
}

}  // namespace color

}  // namespace fridge
