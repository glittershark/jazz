#include "daisy_seed.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

DaisySeed hw;

// Simple one-pole highpass filter state
static float last_output = 0.0f;
static float last_input = 0.0f;
static float alpha = 0.1f; // Filter coefficient (0.0 = no filtering, 1.0 = full filtering)

float highpass(float sample) {
  // First-order IIR highpass filter: y[n] = alpha * (y[n-1] + x[n] - x[n-1])
  last_output = alpha * (last_output + sample - last_input);
  last_input = sample;
  return last_output;
}

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out,
                   size_t size) {
  for (size_t i = 0; i < size; i++) {
    auto sample = in[0][i];
    float filtered_sample = highpass(sample);
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
    // Lower values = less filtering (lower cutoff), higher values = more filtering (higher cutoff)
    alpha = hw.adc.GetFloat(0);
    System::Delay(1);
  }
}

