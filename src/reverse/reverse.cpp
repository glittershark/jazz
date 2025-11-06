#include "daisy_seed.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

Oscillator osc;
DaisySeed hw;

#define BUFFER_LEN 22000
#define FEEDBACK 0.3

static float buffer[BUFFER_LEN] = {};
static int write_pos = 0;

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out,
                   size_t size) {
  for (size_t i = 0; i < size; i++) {
    auto sample = in[0][i] / 2.0;
    auto read_pos = BUFFER_LEN - write_pos - 1;
    auto delayed_signal = buffer[read_pos];
    buffer[write_pos] = sample + (delayed_signal * FEEDBACK);
    write_pos = (write_pos + 1) % (BUFFER_LEN);

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
