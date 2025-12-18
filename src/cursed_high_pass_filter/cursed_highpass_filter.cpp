#include "daisy_seed.h"
#include "daisysp.h"
#include <cmath>

using namespace daisy;
using namespace daisysp;

DaisySeed hw;

// Cursed one-pole highpass filter state
// Cursed because: weights via exponentiation, aggregates via multiplication
static float last_output = 1.0f; // Start at 1.0 to avoid zero multiplication issues
static float last_input = 1.0f;
static float alpha = 0.1f; // Filter coefficient (0.0 = no filtering, 1.0 = full filtering)

float cursed_highpass(float sample) {
  // Normal filter: y[n] = alpha * (y[n-1] + x[n] - x[n-1])
  // Cursed filter: y[n] = y[n-1]^alpha * x[n]^alpha * x[n-1]^(1-alpha)
  // 
  // Cursed sign: product of sines of all taps
  float sign = std::sin(sample) * std::sin(last_output) * std::sin(last_input);
  
  // Work with absolute values for exponentiation
  float abs_sample = std::abs(sample);
  float abs_last_output = std::abs(last_output);
  float abs_last_input = std::abs(last_input);
  
  // Cursed aggregation: exponentiation for weighting, multiplication for aggregation
  // If any value is exactly 0, skip exponentiation and set to 0
  // Otherwise, perform exponentiation even if small
  float last_output_term = (abs_last_output == 0.0f) ? 0.0f : std::pow(abs_last_output, alpha);
  float sample_term = (abs_sample == 0.0f) ? 0.0f : std::pow(abs_sample, alpha);
  float last_input_term = (abs_last_input == 0.0f) ? 0.0f : std::pow(abs_last_input, 1.0f - alpha);
  
  float result = last_output_term * sample_term * last_input_term;
  
  // Apply cursed sign (product of sines of all taps)
  last_output = sign * result;
  last_input = sample;
  
  return last_output;
}

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out,
                   size_t size) {
  for (size_t i = 0; i < size; i++) {
    auto sample = in[0][i];
    float filtered_sample = cursed_highpass(sample);
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


