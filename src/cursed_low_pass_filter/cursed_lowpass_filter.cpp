#include <cmath>

#include "daisy_seed.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

DaisySeed hw;

// Cursed one-pole lowpass filter state
// Cursed because: weights via exponentiation, aggregates via multiplication
static float last_output =
    1.0f;  // Start at 1.0 to avoid zero multiplication issues
static float alpha =
    0.1f;  // Filter coefficient (0.0 = no filtering, 1.0 = full filtering)

float cursed_lowpass(float sample) {
  // Normal filter: y[n] = x[n] * alpha + y[n-1] * (1 - alpha)
  // Cursed filter: y[n] = x[n]^alpha * y[n-1]^(1 - alpha)
  //
  // Cursed sign: product of sines of all taps
  float sign = std::sin(sample) * std::sin(last_output);

  // Work with absolute values for exponentiation
  float abs_sample = std::abs(sample);
  float abs_last = std::abs(last_output);

  // Cursed aggregation: exponentiation for weighting, multiplication for
  // aggregation If abs_sample is exactly 0, skip exponentiation and set to 0
  // Otherwise, perform exponentiation even if small
  float sample_term = (abs_sample == 0.0f) ? 0.0f : std::pow(abs_sample, alpha);
  float last_term =
      (abs_last == 0.0f) ? 0.0f : std::pow(abs_last, 1.0f - alpha);

  float result = sample_term * last_term;

  // Apply cursed sign (product of sines of all taps)
  last_output = sign * result;

  return last_output;
}

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out,
                   size_t size) {
  for (size_t i = 0; i < size; i++) {
    auto sample = in[0][i];
    float filtered_sample = cursed_lowpass(sample);
    out[0][i] = filtered_sample;
  }
}

int main() {
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
    // Lower values = more filtering (lower cutoff), higher values = less
    // filtering (higher cutoff)
    alpha = hw.adc.GetFloat(0);
    System::Delay(1);
  }
}
