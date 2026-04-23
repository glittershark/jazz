#ifndef UNIT_TEST

#include <cassert>
#include <cstddef>

#include "config.hpp"
#include "daisy_seed.h"
#include "engine.hpp"
#include "sound.hpp"

using namespace fridge;
using namespace daisy;

DaisySeed hw;
config::Config config;
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
