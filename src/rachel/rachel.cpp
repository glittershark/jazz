#include "daisy_seed.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

Oscillator lfo; // LFO for modulation
DaisySeed hw;

#define BUFFER_LEN 44100 * 2
#define FEEDBACK 0.3

static float buffer[BUFFER_LEN] = {};
static int buffer_pos = 0;
static int delay_samples = 22000;
static float sample_rate;

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out,
                   size_t size) {
  for (size_t i = 0; i < size; i++) {
    float lfo_val = lfo.Process();

    float delay_mod = delay_samples * (1.0f + lfo_val * 0.1f); // 10% modulation
    int delay_samples = (int)delay_mod;

    int read_pos = buffer_pos % (delay_samples - 1);

    auto sample = in[0][i] / 2.0f;
    auto delayed_signal = buffer[read_pos];

    buffer[buffer_pos] = sample + (delayed_signal * FEEDBACK);
    buffer_pos = (buffer_pos + 1) % (delay_samples - 1
                                     // BUFFER_LEN
                                    );

    out[0][i] = sample + delayed_signal;
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
