#include "engine.hpp"

#include <cstddef>

#include "config.hpp"
#include "libjazz/units.hpp"

#ifndef UNIT_TEST

#include "daisy_seed.h"
#include "io.hpp"

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

namespace {
constexpr const size_t kSW0 = 0;
constexpr const size_t kSW1 = 1;
constexpr const size_t kKNOB_A0 = 2;
constexpr const size_t kKNOB_B0 = 3;
constexpr const size_t kKNOB_A1 = 4;
constexpr const size_t kKNOB_B1 = 5;
constexpr const size_t kKNOB_S0 = 6;
constexpr const size_t kKNOB_S1 = 7;

enum class KnobBank { K0, K1 };

static size_t knob_a(KnobBank mux) {
  switch (mux) {
  case KnobBank::K0:
    return kKNOB_A0;
  case KnobBank::K1:
    return kKNOB_A1;
  default:
    assert(false);
  }
}

static size_t knob_b(KnobBank mux) {
  switch (mux) {
  case KnobBank::K0:
    return kKNOB_B0;
  case KnobBank::K1:
    return kKNOB_B1;
  default:
    assert(false);
  }
}

static io::QuadratureEncoder knob(io::mux::MultiGpioInMux<Engine::kMuxes>* mux,
                                  KnobBank bank, size_t channel) {
  return {mux->channel(knob_a(bank), channel),
          mux->channel(knob_b(bank), channel)};
}
}  // namespace

Engine::Engine(config::Config initial_config)
    // TODO: assign the right pins in here
    : mux_({
          /* 0 =*/io::mux::GpioInMux(D3),   // SW0 in the schematic
          /* 1 =*/io::mux::GpioInMux(D4),   // SW1 in the schematic
          /* 2 =*/io::mux::GpioInMux(D5),   // KNOB_A0 in the schematic
          /* 3 =*/io::mux::GpioInMux(D6),   // KNOB_B0 in the schematic
          /* 4 =*/io::mux::GpioInMux(D7),   // KNOB_A1 in the schematic
          /* 5 =*/io::mux::GpioInMux(D8),   // KNOB_B1 in the schematic
          /* 6 =*/io::mux::GpioInMux(D9),   // KNOB_S0 in the schematic
          /* 7 =*/io::mux::GpioInMux(D10),  // KNOB_S1 in the schematic
      }),

      scan_(io::mux::Address(D0, D1, D2), &mux_),

      timer_(TimerHandle::Config::Peripheral::TIM_3, scan_.GetCallback()),

      // dry = SW2
      dry_(knob(&mux_, KnobBank::K0, 0)),
      // wet = SW3
      wet_(knob(&mux_, KnobBank::K0, 1)),

      head_{
          // position = SW4
          .position = knob(&mux_, KnobBank::K0, 2),
          // write_amount = SW5
          .write_amount = knob(&mux_, KnobBank::K0, 3),
          // read_amount = SW6
          .read_amount = knob(&mux_, KnobBank::K0, 4),
          // erase_amount = Sw7
          .erase_amount = knob(&mux_, KnobBank::K0, 5),
          // feedback = SW8
          .feedback = knob(&mux_, KnobBank::K0, 6),
          // SW9 is spare
      },

      lfo_{
          // range = SW10
          .range = knob(&mux_, KnobBank::K1, 0),
          // range = SW11
          .max_grain_size = knob(&mux_, KnobBank::K1, 1),
          // range = SW12
          .min_grain_size = knob(&mux_, KnobBank::K1, 2),
          // range = SW13
          .reverse_chance = knob(&mux_, KnobBank::K1, 3),
          // range = SW14
          .teleport_chance = knob(&mux_, KnobBank::K1, 4),
          // range = SW15
          .pitch_shift_chance = knob(&mux_, KnobBank::K1, 5),
          // range = SW16
          .low_octave_chance = knob(&mux_, KnobBank::K1, 6),
          // range = SW17
          .high_octave_chance = knob(&mux_, KnobBank::K1, 7),
      },

      // head selection is on the SW0 mux
      head_select_{
          mux_.channel(kSW0, 0), mux_.channel(kSW0, 1), mux_.channel(kSW0, 2),
          mux_.channel(kSW0, 3), mux_.channel(kSW0, 4), mux_.channel(kSW0, 5),
          mux_.channel(kSW0, 6), mux_.channel(kSW0, 7),
      },

      // lfo selection is on the SW1 mux
      lfo_select_{
          mux_.channel(kSW1, 0), mux_.channel(kSW1, 1), mux_.channel(kSW1, 2),
          mux_.channel(kSW1, 3), mux_.channel(kSW1, 4), mux_.channel(kSW1, 5),
          mux_.channel(kSW1, 6), mux_.channel(kSW1, 7),
      },

      leds_({
          .interrupt = D13,
          .shutdown = D14,

      }),

      ui_(leds_, initial_config) {
  // assign physical controls to UI controls
  head_.position.OnChange(ui_.head.position.GetCallback());
  head_.write_amount.OnChange(ui_.head.write_amount.GetCallback());
  head_.read_amount.OnChange(ui_.head.read_amount.GetCallback());
  head_.erase_amount.OnChange(ui_.head.erase_amount.GetCallback());
  head_.feedback.OnChange(ui_.head.feedback.GetCallback());

  dry_.OnChange(ui_.dry.GetCallback());
  wet_.OnChange(ui_.wet.GetCallback());

  lfo_.range.OnChange(ui_.lfo.range.GetCallback());
  lfo_.max_grain_size.OnChange(ui_.lfo.max_grain_size.GetCallback());
  lfo_.min_grain_size.OnChange(ui_.lfo.min_grain_size.GetCallback());
  lfo_.reverse_chance.OnChange(ui_.lfo.reverse_chance.GetCallback());
  lfo_.teleport_chance.OnChange(ui_.lfo.teleport_chance.GetCallback());
  lfo_.pitch_shift_chance.OnChange(ui_.lfo.pitch_shift_chance.GetCallback());
  lfo_.low_octave_chance.OnChange(ui_.lfo.low_octave_chance.GetCallback());
  lfo_.high_octave_chance.OnChange(ui_.lfo.high_octave_chance.GetCallback());

  // and then physical buttons (registers in the order you would expect)
  ui_.head_select.RegisterCallbacks(head_select_);
  ui_.lfo_select.RegisterCallbacks(lfo_select_);

  // TODO: tempo button should probably be connected to something lol

  timer_.Start();
}

const fridge::transition::Frame& Engine::Tick(Samples<uint32_t> dt) {
  const auto& config = ui_.Config();
  const auto& output = transform_.Update(config, dt);
  return head_transitions_.Update(output, transform_.head_transitions());
}

#endif  // UNIT_TEST
