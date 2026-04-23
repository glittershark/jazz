#include "engine.hpp"

#ifndef UNIT_TEST

#include "color.hpp"
#include "daisy_seed.h"
#include "io.hpp"

using namespace daisy;
using namespace daisy::seed;
using namespace fridge;
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

Engine::Knobs::Head::Head(io::mux::MultiGpioInMux<2>& mux, ui::Head& head)
    : position(mux.channel(0, 0), mux.channel(1, 0)),
      write_amount(mux.channel(0, 1), mux.channel(1, 1)),
      read_amount(mux.channel(0, 2), mux.channel(1, 2)),
      erase_amount(mux.channel(0, 3), mux.channel(1, 3)),
      feedback(mux.channel(0, 4), mux.channel(1, 4)) {
  position.OnChange(head.position.GetCallback());
  write_amount.OnChange(head.write_amount.GetCallback());
  read_amount.OnChange(head.read_amount.GetCallback());
  erase_amount.OnChange(head.erase_amount.GetCallback());
  position.OnChange(head.position.GetCallback());
}

Engine::Knobs::LFO::LFO(io::mux::MultiGpioInMux<2>& mux, ui::LFO& lfo)
    : range(mux.channel(0, 0), mux.channel(1, 0)),
      max_grain_size(mux.channel(0, 1), mux.channel(1, 1)),
      min_grain_size(mux.channel(0, 2), mux.channel(1, 2)),
      reverse_chance(mux.channel(0, 3), mux.channel(1, 3)),
      teleport_chance(mux.channel(0, 4), mux.channel(1, 4)),
      pitch_shift_chance(mux.channel(0, 5), mux.channel(1, 5)),
      low_octave_chance(mux.channel(0, 6), mux.channel(1, 6)),
      high_octave_chance(mux.channel(0, 7), mux.channel(1, 7)) {
  range.OnChange(lfo.range.GetCallback());
  max_grain_size.OnChange(lfo.max_grain_size.GetCallback());
  min_grain_size.OnChange(lfo.min_grain_size.GetCallback());
  reverse_chance.OnChange(lfo.reverse_chance.GetCallback());
  teleport_chance.OnChange(lfo.teleport_chance.GetCallback());
  pitch_shift_chance.OnChange(lfo.pitch_shift_chance.GetCallback());
  low_octave_chance.OnChange(lfo.low_octave_chance.GetCallback());
  high_octave_chance.OnChange(lfo.high_octave_chance.GetCallback());
}

Engine::Knobs::Knobs(Engine::Muxes& muxes, ui::UI& ui)
    : head(muxes.bank_a.mux, ui.head),
      dry(muxes.bank_a.mux.channel(0, 5), muxes.bank_a.mux.channel(1, 5)),
      wet(muxes.bank_a.mux.channel(0, 6), muxes.bank_a.mux.channel(1, 6)),
      lfo(muxes.bank_b.mux, ui.lfo) {
  dry.OnChange(ui.dry.GetCallback());
  wet.OnChange(ui.dry.GetCallback());
}

Engine::Engine()
    // TODO: assign the right pins in here
    : muxes_{
          .bank_a{
              // NOTE: i'm relatively sure this is /not/ UB
              .mux = io::mux::MultiGpioInMux<2>({
                  io::mux::GpioInMux(D1),
                  io::mux::GpioInMux(D2),
              }),
              .scan = io::mux::channel_scan::make<8>(
                  io::mux::Address(D3, D4, D5),
                  TimerHandle::Config::Peripheral::TIM_3, muxes_.bank_a.mux),
          },

          .bank_b{
              .mux = io::mux::MultiGpioInMux<2>({
                  io::mux::GpioInMux(D6),
                  io::mux::GpioInMux(D7),
              }),
              .scan = io::mux::channel_scan::make<8>(
                  io::mux::Address(D8, D9, D10),
                  TimerHandle::Config::Peripheral::TIM_4, muxes_.bank_b.mux),
          },
    },

      // this kills the clang-format
      knobs_(muxes_, ui_) {}

void Engine::Tick() {
  config_ |= ui_;  // lmao
}

#endif  // UNIT_TEST
