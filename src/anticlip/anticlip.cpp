#include "daisy_seed.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

DaisySeed hw;

static float threshold = 0.5;

float clip(float sample) {
  if (sample > threshold) {
    return threshold;
  } else if (sample < -threshold) {
    return -threshold;
  }
  return sample;
}

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out,
                   size_t size) {
  for (size_t i = 0; i < size; i++) {
    // TODO: Add your clipping DSP here
    auto sample = in[0][i];
    float clipped_sample = clip(sample);
    float residue = sample - clipped_sample;
    out[0][i] = sample + residue;
  }
}

int main(void) {
  hw.Configure();
  hw.Init();
  hw.SetAudioBlockSize(4);

  AdcChannelConfig adcConfig;
  adcConfig.InitSingle(hw.GetPin(21));
  hw.adc.Init(&adcConfig, 1);
  hw.adc.Start();

  hw.StartAudio(AudioCallback);
  for (;;) {
    // TODO: Read ADC values for parameter control
    System::Delay(1);
  }
}

