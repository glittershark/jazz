#ifndef RGB_LED_H_
#define RGB_LED_H_

#include "value_display.hpp"
#ifndef UNIT_TEST

#include "color.hpp"
#include "led.hpp"

namespace fridge::ui {

/* Bound to the lifetime of a referred fridge::io::led::Controller */
class RgbLed {
  fridge::io::led::Controller::Led red_;
  fridge::io::led::Controller::Led green_;
  fridge::io::led::Controller::Led blue_;

 public:
  RgbLed(fridge::io::led::Controller::Led red,
         fridge::io::led::Controller::Led green,
         fridge::io::led::Controller::Led blue)
      : red_(red), green_(green), blue_(blue) {};

  RgbLed& SetOn(bool on);
  RgbLed& SetColor(fridge::color::RGB color);
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

#endif  // UNIT_TEST

#endif  // RGB_LED_H_
