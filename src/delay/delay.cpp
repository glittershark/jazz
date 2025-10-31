#include "daisy_seed.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

Oscillator osc;
DaisySeed hw;

#define BUFFER_LEN 22000
#define FEEDBACK 0.3

static float buffer[BUFFER_LEN] = {};
static int buffer_pos = 0;

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out,
                   size_t size) {
  for (size_t i = 0; i < size; i++) {
    auto sample = in[0][i] / 2.0;
    auto delayed_signal = buffer[buffer_pos];
    buffer[buffer_pos] = sample + (delayed_signal * FEEDBACK);
    buffer_pos = (buffer_pos + 1) % (BUFFER_LEN - 1);

    out[0][i] = sample + delayed_signal;
  }
}

int main(void) {
  hw.Configure();
  hw.Init();
  hw.SetAudioBlockSize(4);
  float sample_rate = hw.AudioSampleRate();

  osc.Init(sample_rate);
  osc.SetFreq(880.f);

  hw.StartAudio(AudioCallback);
  while (1) {
  }
}
