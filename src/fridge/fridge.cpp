#include "fridge.hpp"

#ifndef UNIT_TEST

#include "daisy_seed.h"

daisy::DaisySeed hw;

int main(void) {
  hw.Init();
  hw.SetAudioBlockSize(8);
  hw.StartLog();
}

#endif
