#ifndef UNIT_TEST

#include <cstddef>

#include "daisy_seed.h"
#include "touch_processor.hpp"

using namespace daisy;

DaisySeed hw;

static touch::Processor processor;

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out,
                   size_t size) {
  for (size_t i = 0; i < size; i++) {
    out[0][i] = processor.Process(in[0][i]);
  }
}

int main() {
  hw.Init();
  hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
  hw.SetAudioBlockSize(4);

  AdcChannelConfig adcConfig;
  adcConfig.InitSingle(hw.GetPin(21));
  hw.adc.Init(&adcConfig, 1);
  hw.adc.Start();

  hw.StartAudio(AudioCallback);
  for (;;) {
    System::Delay(1000);
  }
}

#endif
