#include "engine.hpp"

#ifndef UNIT_TEST

#include "color.hpp"
#include "daisy_seed.h"

using namespace daisy;
using namespace daisy::seed;
using namespace fridge::engine;

BreadboardEngine::BreadboardEngine()
    : encoders_({io::mux::GpioInMux(D4), io::mux::GpioInMux(D3)}),
      scan_(io::mux::channel_scan::make<8>(
          io::mux::Address(
              /* a = */ D15,
              /* b = */ D16,
              /* c = */ D17),
          TimerHandle::Config::Peripheral::TIM_3, encoders_)),
      enc1_(encoders_.channel(0, 2), encoders_.channel(1, 2)),
      enc2_(encoders_.channel(0, 0), encoders_.channel(1, 0)) {
  enc1_.OnChange(knob1_.GetCallback());
  enc2_.OnChange(knob2_.GetCallback());
  knob1_.Set(0);
  knob2_.Set(0);
}

void BreadboardEngine::Tick() {
  {
    color::RGB color = color::HSV(knob1_.Get(), 255, 255);
    led_controller_.B(0, 1).SetOn(true).SetPwm(color.red);
    led_controller_.B(1, 1).SetOn(true).SetPwm(color.blue);
    led_controller_.B(2, 1).SetOn(true).SetPwm(color.green);
  }

  {
    color::RGB color = color::HSV(knob2_.Get(), 255, 255);
    led_controller_.B(4, 5).SetOn(true).SetPwm(color.red);
    led_controller_.B(5, 5).SetOn(true).SetPwm(color.blue);
    led_controller_.B(6, 5).SetOn(true).SetPwm(color.green);
  }
}

#endif  // UNIT_TEST
