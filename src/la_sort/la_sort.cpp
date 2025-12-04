#include "daisy_seed.h"
#include "daisysp.h"
#include <cmath>

using namespace daisy;
using namespace daisysp;

DaisySeed hw;

// Fixed-size buffers (no dynamic allocation)
static constexpr size_t BUFFER_SIZE = 64;
static float insertion_buffer[BUFFER_SIZE] = {0.0f}; // Circular buffer in insertion order
static float sorted_samples[BUFFER_SIZE] = {0.0f};  // Sorted array
static size_t current_size = 0;
static size_t write_index = 0; // Circular buffer write position

// Weight parameters
static float weight_center = 0.5f; // ADC control: 0.0 = weight edges, 1.0 = weight center
static float weight_sharpness = 2.0f; // How sharp the weighting curve is

// Remove a value from sorted array
void remove_from_sorted(float value) {
  // Find the value in sorted array (handle duplicates by removing first occurrence)
  size_t remove_pos = 0;
  for (size_t i = 0; i < current_size; i++) {
    if (sorted_samples[i] == value) {
      remove_pos = i;
      break;
    }
  }
  
  // Shift elements to remove
  for (size_t i = remove_pos; i < current_size - 1; i++) {
    sorted_samples[i] = sorted_samples[i + 1];
  }
  current_size--;
}

// Insert sample into sorted array while maintaining sorted order
void insert_sorted(float sample) {
  if (current_size < BUFFER_SIZE) {
    // Buffer not full yet, insert in sorted position
    size_t insert_pos = current_size;
    for (size_t i = 0; i < current_size; i++) {
      if (sample < sorted_samples[i]) {
        insert_pos = i;
        break;
      }
    }
    
    // Shift elements to make room
    for (size_t i = current_size; i > insert_pos; i--) {
      sorted_samples[i] = sorted_samples[i - 1];
    }
    sorted_samples[insert_pos] = sample;
    current_size++;
    
    // Also add to insertion buffer
    insertion_buffer[current_size - 1] = sample;
  } else {
    // Buffer is full, replace oldest sample
    float oldest_sample = insertion_buffer[write_index];
    
    // Remove oldest from sorted array
    remove_from_sorted(oldest_sample);
    
    // Insert new sample in sorted position
    size_t insert_pos = current_size; // current_size is now BUFFER_SIZE - 1
    for (size_t i = 0; i < current_size; i++) {
      if (sample < sorted_samples[i]) {
        insert_pos = i;
        break;
      }
    }
    
    // Shift elements to make room
    for (size_t i = current_size; i > insert_pos; i--) {
      sorted_samples[i] = sorted_samples[i - 1];
    }
    sorted_samples[insert_pos] = sample;
    current_size++; // Restore to BUFFER_SIZE
    
    // Update insertion buffer (circular)
    insertion_buffer[write_index] = sample;
    write_index = (write_index + 1) % BUFFER_SIZE;
  }
}

// Compute weighted sum of sorted samples
float weighted_tap() {
  if (current_size == 0) {
    return 0.0f;
  }
  
  float sum = 0.0f;
  float weight_sum = 0.0f;
  
  for (size_t i = 0; i < current_size; i++) {
    // Normalize position to [0, 1] in sorted array
    float normalized_pos = static_cast<float>(i) / static_cast<float>(current_size - 1);
    
    // Compute weight based on position
    // weight_center controls where peak weight is:
    //   0.0 = weight minimum values (left edge)
    //   0.5 = weight center/median values
    //   1.0 = weight maximum values (right edge)
    float distance_from_target = std::abs(normalized_pos - weight_center);
    
    // Exponential weighting: closer to target position = higher weight
    float weight = std::exp(-weight_sharpness * distance_from_target);
    
    sum += sorted_samples[i] * weight;
    weight_sum += weight;
  }
  
  // Normalize by sum of weights
  return (weight_sum > 0.0f) ? (sum / weight_sum) : 0.0f;
}

float la_sort(float sample) {
  insert_sorted(sample);
  return weighted_tap();
}

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out,
                   size_t size) {
  for (size_t i = 0; i < size; i++) {
    auto sample = in[0][i];
    float processed_sample = la_sort(sample);
    out[0][i] = processed_sample;
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
    // Map ADC value (0.0-1.0) to weight center position
    // 0.0 = weight minimum values (left edge of sorted array)
    // 0.5 = weight center/median values
    // 1.0 = weight maximum values (right edge of sorted array)
    weight_center = hw.adc.GetFloat(0);
    System::Delay(1);
  }
}

