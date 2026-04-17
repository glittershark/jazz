#ifndef UNIT_TEST

#include <cassert>

#include "daisy_seed.h"
#include "io.hpp"
#include "ui.hpp"

using namespace daisy;
using namespace daisy::seed;
using namespace fridge;

DaisySeed hw;

int main() {
  hw.Init();
  hw.SetAudioBlockSize(8);
  hw.StartLog(false);

  io::mux::MultiGpioInMux<2> encoders({
      fridge::io::mux::GpioInMux(D4),
      fridge::io::mux::GpioInMux(D3),
  });

  auto scan = io::mux::channel_scan::make<8>(
      io::mux::Address(
          /* a = */ D15,
          /* b = */ D16,
          /* c = */ D17),
      TimerHandle::Config::Peripheral::TIM_3, encoders);

  auto enc1 =
      io::QuadratureEncoder(encoders.channel(0, 2), encoders.channel(1, 2));
  auto enc2 =
      io::QuadratureEncoder(encoders.channel(0, 0), encoders.channel(1, 0));

  scan.Start();

  ui::Knob<ui::Size> knob1;
  ui::Knob<ui::Size> knob2;

  enc1.OnChange(knob1.GetCallback());
  enc2.OnChange(knob2.GetCallback());

  knob1.Set(0);
  knob2.Set(0);

  hw.PrintLine("Now cycling hue with encoder...");

  fridge::io::led::Controller controller;

  for (;;) {
    controller.B(0, 1).On(true).Pwm(knob1.Get());
    controller.B(4, 5).On(true).Pwm(knob2.Get());
    System::Delay(10);
  }
}

#endif
