#ifndef UNIT_TEST

#include <cassert>

#include "config.hpp"
#include "daisy_seed.h"
#include "engine.hpp"

using namespace fridge;

daisy::DaisySeed hw;
config::Config config;

[[noreturn]] void breadboard() {
  engine::BreadboardEngine engine;

  hw.PrintLine("Now cycling hue with encoder...");

  fridge::io::led::Controller controller;

  for (;;) {
    engine();
    daisy::System::Delay(10);
  }
}

[[noreturn]] void actual_fridge() {
  engine::Engine engine;

  hw.PrintLine("Now refrigerating your heads...");

  for (;;) {
    engine(::config);
    daisy::System::Delay(10);
  }
}

[[noreturn]] int main() {
  hw.Init();
  hw.SetAudioBlockSize(8);
  hw.StartLog(false);

  actual_fridge();
}

#endif
