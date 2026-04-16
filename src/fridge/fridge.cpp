#ifndef UNIT_TEST

#include <cassert>

#include "daisy_seed.h"
#include "io.hpp"

namespace fridge {}  // namespace fridge

using namespace daisy;
using namespace daisy::seed;

DaisySeed hw;

int main() {
  hw.Init();
  hw.SetAudioBlockSize(8);
  hw.StartLog(false);

  // fridge::io::mux::MultiGpioInMux<2> encoders({
  //     fridge::io::mux::GpioInMux(D4),
  //     fridge::io::mux::GpioInMux(D3),
  // });

  // auto scan = fridge::io::mux::channel_scan::make<8>(
  //     fridge::io::mux::Address(
  //         /* a = */ D15,
  //         /* b = */ D16,
  //         /* c = */ D17),
  //     TimerHandle::Config::Peripheral::TIM_3, encoders);

  // auto enc1 = fridge::io::QuadratureEncoder(encoders.channel(0, 2),
  //                                           encoders.channel(1, 2));
  // auto enc2 = fridge::io::QuadratureEncoder(encoders.channel(0, 0),
  //                                           encoders.channel(1, 0));

  // fridge::io::mux::Address led_address(D9, D10, D11);
  // led_address.SelectChannel(2);

  // // RGB LED on TIM4 (common anode, so inverted)
  // // D14 (PB7) = TIM4_CH2 = Red
  // // D13 (PB6) = TIM4_CH1 = Green
  // // D12 (PB9) = TIM4_CH4 = Blue
  // pwm::Timer pwm(TimerHandle::Config::Peripheral::TIM_4);
  // auto red = pwm.InitChannel(/* channel = */ 2, /* pin = */ D14);
  // auto green = pwm.InitChannel(/* channel = */ 1, /* pin = */ D13);
  // auto blue = pwm.InitChannel(/* channel = */ 4, /* pin = */ D12);

  // scan.Start();

  // // Debug: try fixed red first (try both polarities)
  // hw.PrintLine("Testing red=255 (ON if common cathode)");
  // red.Set(255);
  // green.Set(0);
  // blue.Set(0);

  // hw.PrintLine("Now cycling hue with encoder...");

  // int32_t prev_ticks = 0;
  //
  fridge::io::led::Controller controller;

  for (int i = 0; i <= 7; i++) {
    for (int j = 0; j <= 7; j++) {
      controller.B(i, j).Pwm(255);
    }
  }

  for (;;) {
    hw.PrintLine("hi!");
    System::Delay(1000);
    for (int i = 0; i <= 7; i++) {
      for (int j = 0; j <= 7; j++) {
        controller.B(i, j).On(true);
      }
    }
    System::Delay(1000);
    for (int i = 0; i <= 7; i++) {
      for (int j = 0; j <= 7; j++) {
        controller.B(i, j).On(false);
      }
    }
  }
}

#endif
