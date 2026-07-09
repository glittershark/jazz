#include "rgb_led.hpp"
#include "value_display.hpp"

#ifndef UNIT_TEST

#include <cassert>
#include <cstddef>

#include "config.hpp"
#include "daisy_seed.h"
#include "default_config.hpp"
#include "engine.hpp"
#include "libjazz/color.hpp"
#include "sound.hpp"

using namespace jazz;
using namespace fridge;
using namespace daisy;

DaisySeed hw;
config::Config config = config::DefaultConfig();
sound::Sound* sound;

char DSY_SDRAM_BSS sound_memory[sizeof(sound::Sound)];

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out,
                   size_t size) {
  for (size_t i = 0; i < size; ++i) {
    out[0][i] = ::sound->ProcessSample(::config, in[0][i]);
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

  AdcChannelConfig adcConfig;
  adcConfig.InitSingle(hw.GetPin(21));
  hw.adc.Init(&adcConfig, 1);
  hw.adc.Start();

  hw.StartAudio(AudioCallback);

  engine::Engine engine;

  hw.PrintLine("Now refrigerating your heads...");

  for (;;) {
    const uint32_t delay_ms = 10;
    const float dt = delay_ms / 1000.f;

    engine.Tick(::config, dt);
    System::Delay(delay_ms);
  }
}

[[noreturn]] int main() {
  hw.Init();
  hw.SetAudioBlockSize(8);
  hw.StartLog(false);

  ActualFridge();
}

#endif
