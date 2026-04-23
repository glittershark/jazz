#ifndef UNIT_TEST

#include <cassert>

#include "daisy_seed.h"
#include "engine.hpp"

using namespace fridge;

daisy::DaisySeed hw;

int main() {
  hw.Init();
  hw.SetAudioBlockSize(8);
  hw.StartLog(false);

  engine::BreadboardEngine engine;

  hw.PrintLine("Now cycling hue with encoder...");

  fridge::io::led::Controller controller;

  for (;;) {
    engine();
    daisy::System::Delay(10);
  }
}

#endif
