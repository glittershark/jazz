#ifndef RGB_LED_H_
#define RGB_LED_H_

#include "color.hpp"
#include "led.hpp"
#include "value_display.hpp"

namespace fridge::ui {

/* Bound to the lifetime of a referred fridge::io::led::Controller */
class RgbLed {
  fridge::io::led::Led red_;
  fridge::io::led::Led green_;
  fridge::io::led::Led blue_;

 public:
  RgbLed(fridge::io::led::Led red, fridge::io::led::Led green,
         fridge::io::led::Led blue)
      : red_(red), green_(green), blue_(blue) {};

  RgbLed& SetOn(bool on);
  RgbLed& SetColor(fridge::color::RGB color);

  fridge::io::led::Led& red() { return red_; }
  fridge::io::led::Led& green() { return green_; }
  fridge::io::led::Led& blue() { return blue_; }
};

template <ValueDisplay VD>
class RgbLedValueDisplay : public RgbLed {
  VD value_display_;

 public:
  RgbLedValueDisplay(VD value_display, RgbLed rgb_led)
      : RgbLed(rgb_led), value_display_(value_display) {};

  RgbLedValueDisplay<VD>& SetValue(uint8_t value) {
    SetColor(value_display_(value));
    return *this;
  }
};

}  // namespace fridge::ui

#endif  // RGB_LED_H_
