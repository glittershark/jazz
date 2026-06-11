#include "engine.hpp"

#include "libjazz/units.hpp"

#ifndef UNIT_TEST

#include "daisy_seed.h"
#include "io.hpp"
#include "libjazz/color.hpp"

using namespace jazz;
using namespace daisy;
using namespace daisy::seed;
using namespace fridge;
using namespace fridge::engine;
using jazz::units::Samples;

Timer::Timer(daisy::TimerHandle::Config::Peripheral timer, Callback<> callback,
             std::chrono::microseconds period)
    : callback_(callback), period_(period) {
  daisy::TimerHandle::Config timer_config;
  timer_config.periph = timer;
  timer_config.enable_irq = true;

  timer_.Init(timer_config);
  timer_.SetCallback(Timer::timer_callback_, this);
  timer_.SetPeriod((period_.count() * timer_.GetFreq()) / 1'000'000);
}

BreadboardEngine::BreadboardEngine()
    : encoders_({io::mux::GpioInMux(D4), io::mux::GpioInMux(D3)}),
      scan_(io::mux::Address(
                /* a = */ D15,
                /* b = */ D16,
                /* c = */ D17),
            encoders_),
      timer_(TimerHandle::Config::Peripheral::TIM_3, scan_.GetCallback()),
      enc1_(encoders_.channel(0, 2), encoders_.channel(1, 2)),
      enc2_(encoders_.channel(0, 0), encoders_.channel(1, 0)) {
  enc1_.OnChange(knob1_.GetCallback());
  enc2_.OnChange(knob2_.GetCallback());
  knob1_.Set(0);
  knob2_.Set(0);
  timer_.Start();
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

Engine::Engine()
    // TODO: assign the right pins in here
    : mux_({
          io::mux::GpioInMux(D1),
          io::mux::GpioInMux(D2),
          io::mux::GpioInMux(D3),
          io::mux::GpioInMux(D4),
          io::mux::GpioInMux(D5),
          io::mux::GpioInMux(D6),
      }),

      scan_(io::mux::Address(D7, D8, D9), mux_),

      timer_(TimerHandle::Config::Peripheral::TIM_3, scan_.GetCallback()),

      // head knobs are on mux 0 & 1
      head_({
          {mux_.channel(0, 0), mux_.channel(1, 0)},
          {mux_.channel(0, 1), mux_.channel(1, 1)},
          {mux_.channel(0, 2), mux_.channel(1, 2)},
          {mux_.channel(0, 3), mux_.channel(1, 3)},
          // feedback =
          {mux_.channel(3, 2), mux_.channel(2, 2)},
      }),

      // dry/wet are also on mux 0 & 1
      dry_(mux_.channel(0, 5), mux_.channel(1, 5)),
      wet_(mux_.channel(3, 0), mux_.channel(2, 0)),
      // one spare channel on mux 0 & 1

      // lfo knobs are on mux 2 & 3
      lfo_({
          {mux_.channel(2, 0), mux_.channel(3, 0)},
          {mux_.channel(2, 1), mux_.channel(3, 1)},
          {mux_.channel(2, 2), mux_.channel(3, 2)},
          {mux_.channel(2, 3), mux_.channel(3, 3)},
          {mux_.channel(2, 4), mux_.channel(3, 4)},
          {mux_.channel(2, 5), mux_.channel(3, 5)},
          {mux_.channel(2, 6), mux_.channel(3, 6)},
          {mux_.channel(2, 7), mux_.channel(3, 7)},
      }),

      // head selection is on mux 4
      head_select_{
          mux_.channel(4, 0), mux_.channel(4, 1), mux_.channel(4, 2),
          mux_.channel(4, 3), mux_.channel(4, 4), mux_.channel(4, 5),
          mux_.channel(4, 6), mux_.channel(4, 7),
      },

      // lfo selection is on mux 5
      lfo_select_{
          mux_.channel(5, 0), mux_.channel(5, 1), mux_.channel(5, 2),
          mux_.channel(5, 3), mux_.channel(5, 4), mux_.channel(5, 5),
          mux_.channel(5, 6), mux_.channel(5, 7),
      },

      ui_(leds_) {
  // assign physical controls to UI controls
  head_[0].OnChange(ui_.head.position.GetCallback());
  head_[1].OnChange(ui_.head.write_amount.GetCallback());
  head_[2].OnChange(ui_.head.read_amount.GetCallback());
  head_[3].OnChange(ui_.head.erase_amount.GetCallback());
  head_[4].OnChange(ui_.head.feedback.GetCallback());

  dry_.OnChange(ui_.dry.GetCallback());
  wet_.OnChange(ui_.wet.GetCallback());

  lfo_[0].OnChange(ui_.lfo.range.GetCallback());
  lfo_[1].OnChange(ui_.lfo.max_grain_size.GetCallback());
  lfo_[2].OnChange(ui_.lfo.min_grain_size.GetCallback());
  lfo_[3].OnChange(ui_.lfo.reverse_chance.GetCallback());
  lfo_[4].OnChange(ui_.lfo.teleport_chance.GetCallback());
  lfo_[5].OnChange(ui_.lfo.pitch_shift_chance.GetCallback());
  lfo_[6].OnChange(ui_.lfo.low_octave_chance.GetCallback());
  lfo_[7].OnChange(ui_.lfo.high_octave_chance.GetCallback());

  // and then physical buttons (registers in the order you would expect)
  ui_.head_select.RegisterCallbacks(head_select_);
  ui_.lfo_select.RegisterCallbacks(lfo_select_);

  // TODO: tempo button should probably be connected to something lol

  timer_.Start();
}

const fridge::transition::Frame& Engine::Tick(const config::Config& config,
                                              Samples<uint32_t> dt) {
  // ui_.UpdateConfig(config);
  const auto output = transform_.Update(config, dt);
  return head_transitions_.Update(output, transform_.head_transitions());
}

#endif  // UNIT_TEST
