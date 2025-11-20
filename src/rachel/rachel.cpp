#include "daisy_seed.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

Oscillator lfo; // LFO for modulation
DaisySeed hw;

#define BUFFER_LEN 22000
#define FEEDBACK 0.9

constexpr const size_t nbuffers = 3;
static float buffer[BUFFER_LEN][nbuffers];
static int buffer_pos[] = {0, 0, 0};
static int delay_samples[] = {12000, 2200, 3333};
static float sample_rate;

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out,
                   size_t size) {
  for (size_t i = 0; i < size; i++) {
    float lfo_val = lfo.Process();

    auto sample = in[0][i] * 0.3f;
    float o = sample;
    for (size_t buf = 0; buf < nbuffers; buf++) {

      float delay_mod =
          delay_samples[buf] * (1.0f + lfo_val * 0.5f); // 10% modulation
      int delay_samples = (int)delay_mod;

      int read_pos = buffer_pos[buf] % (delay_samples - 1);

      auto delayed_signal = buffer[buf][read_pos];

      buffer[buf][buffer_pos[buf]] = sample + (delayed_signal * FEEDBACK);
      buffer_pos[buf] = (buffer_pos[buf] + 1) % (delay_samples - 1
                                                 // BUFFER_LEN
                                                );
      o += delayed_signal;
    }
    if (o > 0.5) {
      o = 0.5 + 0.8 * (o - 0.5);
    }

    out[0][i] = o;
  }
}

int main(void) {
  hw.Configure();
  hw.Init();
  hw.SetAudioBlockSize(4);
  sample_rate = hw.AudioSampleRate();

  // Initialize LFO (slow oscillation, like 0.5-2 Hz)
  lfo.Init(sample_rate);
  lfo.SetFreq(0.5f); // 1 Hz oscillation
  lfo.SetWaveform(Oscillator::WAVE_SIN);
  lfo.SetAmp(1.0f);

  hw.StartAudio(AudioCallback);
  for (;;) {
  }
}
