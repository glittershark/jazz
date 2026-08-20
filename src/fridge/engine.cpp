#include "engine.hpp"

#include <cstddef>

#include "callback.hpp"
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

static size_t knob_a(KnobBank bank) {
  switch (bank) {
  case KnobBank::K0:
    return kKNOB_A0;
  case KnobBank::K1:
    return kKNOB_A1;
  default:
    assert(false);
  }
}

static size_t knob_b(KnobBank bank) {
  switch (bank) {
  case KnobBank::K0:
    return kKNOB_B0;
  case KnobBank::K1:
    return kKNOB_B1;
  default:
    assert(false);
  }
}

static size_t knob_s(KnobBank bank) {
  switch (bank) {
  case KnobBank::K0:
    return kKNOB_S0;
  case KnobBank::K1:
    return kKNOB_S1;
  default:
    assert(false);
  }
}

static io::PushbuttonQuadratureEncoder knob(
    io::mux::MultiGpioInMux<Engine::kMuxes>* mux, KnobBank bank,
    size_t channel) {
  return io::PushbuttonQuadratureEncoder({
      .a = mux->channel(knob_a(bank), channel),
      .b = mux->channel(knob_b(bank), channel),
      .button = mux->channel(knob_s(bank), channel),
  });
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
      dry_(knob(&mux_, KnobBank::K0, 1)),
      // wet = SW3
      wet_(knob(&mux_, KnobBank::K0, 0)),

      head_{// position = SW4
            .position = knob(&mux_, KnobBank::K0, 2),
            // write_amount = SW5
            .write_amount = knob(&mux_, KnobBank::K0, 3),
            // read_amount = SW6
            .read_amount = knob(&mux_, KnobBank::K0, 4),
            // erase_amount = Sw7
            .erase_amount = knob(&mux_, KnobBank::K0, 5),
            // feedback = SW8
            .feedback = knob(&mux_, KnobBank::K0, 6),
            // pan = SW9
            .pan = knob(&mux_, KnobBank::K0, 7)},

      lfo_{
          // range = SW10
          .range = knob(&mux_, KnobBank::K1, 0),
          // max_grain_size = SW11
          .max_grain_size = knob(&mux_, KnobBank::K1, 1),
          // min_grain_size = SW12
          .min_grain_size = knob(&mux_, KnobBank::K1, 2),
          // reverse_chance = SW13
          .reverse_chance = knob(&mux_, KnobBank::K1, 3),
          // teleport_chance = SW14
          .teleport_chance = knob(&mux_, KnobBank::K1, 4),
          // pitch_shift_chance = SW15
          .pitch_shift_chance = knob(&mux_, KnobBank::K1, 5),
          // low_octave_chance = SW16
          .low_octave_chance = knob(&mux_, KnobBank::K1, 6),
          // high_octave_chance = SW17
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

      config_(initial_config),

      leds_({
          .interrupt = D13,
          .shutdown = D14,

      }),

      ui_(leds_, &config_) {
  modulator_.SetConfig(config_.Read());
  // assign physical controls to UI controls

#define ON_PRESS_CHANGED(knob, param)                             \
  knob.OnPressChanged(TypedCallback<ui::UI, bool>{                \
      .callback = (+[](ui::UI* ui, bool pressed) {                \
        ui->KnobPressed(config::TargetParameter::param, pressed); \
      }),                                                         \
      .data = &ui_,                                               \
  })

  head_.position.OnChange(ui_.head_knobs().position.GetCallback());
  ON_PRESS_CHANGED(head_.position, kPosition);
  head_.write_amount.OnChange(ui_.head_knobs().write_amount.GetCallback());
  ON_PRESS_CHANGED(head_.write_amount, kWriteAmount);
  head_.read_amount.OnChange(ui_.head_knobs().read_amount.GetCallback());
  ON_PRESS_CHANGED(head_.read_amount, kReadAmount);
  head_.erase_amount.OnChange(ui_.head_knobs().erase_amount.GetCallback());
  ON_PRESS_CHANGED(head_.erase_amount, kEraseAmount);
  head_.feedback.OnChange(ui_.head_knobs().feedback.GetCallback());
  ON_PRESS_CHANGED(head_.feedback, kFeedbackAmount);
  head_.pan.OnChange(ui_.head_knobs().pan.GetCallback());
  ON_PRESS_CHANGED(head_.pan, kPan);

  dry_.OnChange(ui_.dry_knob().GetCallback());
  ON_PRESS_CHANGED(dry_, kDry);
  wet_.OnChange(ui_.wet_knob().GetCallback());
  ON_PRESS_CHANGED(wet_, kWet);

  lfo_.range.OnChange(ui_.lfo_knobs().range.GetCallback());
  ON_PRESS_CHANGED(lfo_.range, kRange);
  lfo_.max_grain_size.OnChange(ui_.lfo_knobs().max_grain_size.GetCallback());
  ON_PRESS_CHANGED(lfo_.max_grain_size, kMaxGrainSize);
  lfo_.min_grain_size.OnChange(ui_.lfo_knobs().min_grain_size.GetCallback());
  ON_PRESS_CHANGED(lfo_.min_grain_size, kMinGrainSize);
  lfo_.reverse_chance.OnChange(ui_.lfo_knobs().reverse_chance.GetCallback());
  ON_PRESS_CHANGED(lfo_.reverse_chance, kReverseChance);
  lfo_.teleport_chance.OnChange(ui_.lfo_knobs().teleport_chance.GetCallback());
  ON_PRESS_CHANGED(lfo_.teleport_chance, kTeleportChance);
  lfo_.pitch_shift_chance.OnChange(
      ui_.lfo_knobs().pitch_shift_chance.GetCallback());
  ON_PRESS_CHANGED(lfo_.pitch_shift_chance, kPitchShiftChance);
  lfo_.low_octave_chance.OnChange(
      ui_.lfo_knobs().low_octave_chance.GetCallback());
  ON_PRESS_CHANGED(lfo_.low_octave_chance, kLowOctaveChance);
  lfo_.high_octave_chance.OnChange(
      ui_.lfo_knobs().high_octave_chance.GetCallback());
  ON_PRESS_CHANGED(lfo_.high_octave_chance, kHighOctaveChance);

  // and then physical buttons (registers in the order you would expect)
  ui_.head_select().RegisterCallbacks(head_select_);
  ui_.lfo_select().RegisterCallbacks(lfo_select_);

  // TODO: tempo button should probably be connected to something lol

  timer_.Start();
}

void Engine::SyncConfig() {
  if (!config_.dirty()) {
    return;
  }

  // The scan IRQ writes the config and the audio IRQ walks the modulator's
  // state; don't let either observe a half-applied swap.
  __disable_irq();
  config_.MarkClean();
  modulator_.SetConfig(config_.Read());
  __enable_irq();
}

#endif  // UNIT_TEST
