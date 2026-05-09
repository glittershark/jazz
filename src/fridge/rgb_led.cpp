#include "rgb_led.hpp"

#include "color.hpp"

namespace fridge::ui {

RgbLed& RgbLed::SetOn(bool on) {
  red_.SetOn(on);
  green_.SetOn(on);
  blue_.SetOn(on);

  return *this;
}

RgbLed& RgbLed::SetColor(fridge::color::RGB color) {
  red_.SetPwm(color.red);
  green_.SetPwm(color.green);
  blue_.SetPwm(color.blue);

  return *this;
}

}  // namespace fridge::ui
