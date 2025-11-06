#include "daisy_seed.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

DaisySeed hw;

#define BUFFER_LEN 44100 * 2 /* 2 seconds */
#define FEEDBACK 0.3

static int delay_samples = 22000;
static float buffer[BUFFER_LEN] = {};
static int read_pos = BUFFER_LEN;
static int write_pos = 0;

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out,
                   size_t size) {
  for (size_t i = 0; i < size; i++) {
    auto sample = in[0][i] * 0.5;
    read_pos = read_pos - 1;
    if (read_pos == -1) {
      read_pos = delay_samples - 1;
    }
    auto delayed_signal = buffer[read_pos];
    buffer[write_pos] = sample + (delayed_signal * FEEDBACK);
    write_pos = (write_pos + 1) % (delay_samples);

    out[0][i] = sample + delayed_signal;
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
    delay_samples = (int)(hw.adc.GetFloat(0) * BUFFER_LEN);
    System::Delay(1);
  }
}
