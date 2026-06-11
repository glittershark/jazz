#include "libjazz/units.hpp"
#include "rgb_led.hpp"
#include "value_display.hpp"

#ifndef UNIT_TEST

#include <cassert>
#include <cstddef>

#include "config.hpp"
#include "daisy_seed.h"
#include "engine.hpp"
#include "libjazz/color.hpp"
#include "sound.hpp"

using namespace jazz;
using namespace fridge;
using namespace daisy;
using jazz::units::Samples;

DaisySeed hw;

config::Config root_config{
    .heads{
        {{.position = 0,
          .write_amount = 1.0f,
          .read_amount = 1.0f,
          .erase_amount = 0.0001f,
          .feedback =
              {
                  .kind = config::Feedback::Kind::kRead,
                  .amount = 0.0f,
              }}},
    },
    .lfos{{{
        .range = 24000,
        .max_grain_size = 12000,
        .min_grain_size = 12000,
        .reverse_chance = 1.0f,
        .teleport_chance = 0.0f,
        .pitch_shift_chance = 0.0f,
        .targets{{{{
            .object = config::TargetObject::kHead,
            .parameter = config::TargetParameter::kPosition,
            .object_idx = 0,
        }}}},
    }}},
    .dry = 0.0f,
    .wet = 1.0f,
};

sound::Sound* sound;
engine::Engine* engine = nullptr;

char DSY_SDRAM_BSS sound_memory[sizeof(sound::Sound)];
char engine_memory[sizeof(engine::Engine)];

constexpr const float kAudioBlockSize = 32;

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out,
                   size_t size) {
  for (size_t i = 0; i < size; ++i) {
    auto frame = ::engine->Tick(::root_config, Samples(1));
    auto res = ::sound->ProcessSample(frame, in[0][i]);
    out[0][i] = res;
  }
}

[[noreturn]] void Breadboard() {
  engine::BreadboardEngine engine;

  hw.PrintLine("Now cycling hue with encoder...");

  fridge::io::led::Controller controller;

  /* This particular choice is quite pretty, but maybe not so legible. */
  ui::value_display::CieInterp blue_to_green{
      .start = color::XYZ(18, 7, 95),
      .end = color::XYZ(35, 71, 12),
  };

  ui::RgbLedValueDisplay<ui::value_display::CieInterp> led1(
      blue_to_green,
      ui::RgbLed(controller.B(0, 1), controller.B(1, 1), controller.B(2, 1)));

  ui::RgbLedValueDisplay<ui::value_display::CieInterp> led2(
      blue_to_green,
      ui::RgbLed(controller.B(4, 5), controller.B(5, 5), controller.B(6, 5)));

  led1.SetOn(true);
  led2.SetOn(true);

  for (;;) {
    engine();
    System::Delay(10);
  }
}

[[noreturn]] void ActualFridge() {
  // XXX: construct this thing BEFORE starting the audio callback!
  ::sound = new (sound_memory) sound::Sound();
  ::engine = new (engine_memory) engine::Engine();

  hw.StartAudio(AudioCallback);

  engine::Engine engine;

  hw.PrintLine("Now refrigerating your heads...");

  for (;;) {
  }
}

[[noreturn]] int main() {
  hw.Init();
  hw.SetAudioBlockSize(8);
  hw.StartLog(false);

  ActualFridge();
}

#endif
