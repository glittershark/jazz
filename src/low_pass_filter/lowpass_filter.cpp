#include "daisy_seed.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

DaisySeed hw;

// Simple one-pole lowpass filter state
static float last_output = 0.0f;
static float alpha = 0.1f; // Filter coefficient (0.0 = no filtering, 1.0 = full filtering)

float lowpass(float sample) {
  // First-order IIR filter: y[n] = x[n] * alpha + y[n-1] * (1 - alpha)
  last_output = sample * alpha + last_output * (1.0f - alpha);
  return last_output;
}

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out,
                   size_t size) {
  for (size_t i = 0; i < size; i++) {
    auto sample = in[0][i];
    float filtered_sample = lowpass(sample);
    out[0][i] = filtered_sample;
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
    // Map ADC value (0.0-1.0) to filter coefficient (0.0-1.0)
    // Lower values = more filtering (lower cutoff), higher values = less filtering (higher cutoff)
    alpha = hw.adc.GetFloat(0);
    System::Delay(1);
  }
}

