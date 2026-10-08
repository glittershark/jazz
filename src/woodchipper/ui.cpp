#include "ui.hpp"

#include "led.hpp"
#include "libjazz/color.hpp"
#include "rgb_led.hpp"
#include "value_display.hpp"

using namespace fridge;
using namespace fridge::ui;

namespace {
constexpr const value_display::CieInterp kBlueToGreen = {
    .start = color::XYZ(18, 7, 95),
    .end = color::XYZ(35, 71, 12),
};
}  // namespace

namespace fridge::ui {

UI::UI(io::led::Controller& led)
    // D10
    : knob_("Knob",
            {kBlueToGreen, RgbLed(led.B(1, 0), led.B(1, 1), led.B(1, 2))}) {
  // Ticks is default-uninitialized; also lights the LED at startup
  knob_.Set(0);
}

}  // namespace fridge::ui
