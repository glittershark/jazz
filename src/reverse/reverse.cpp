#include "daisy_seed.h"
#include "daisysp.h"
#include "hid/logger.h"
#include "libjazz/value.hpp"

using namespace daisy;
using namespace daisysp;

DaisySeed hw;

#define BUFFER_LEN 44100 * 2 /* 2 seconds */

static int delay_samples = 22000;
static int feedback = 0.3;
static float buffer[BUFFER_LEN] = {};
static int read_pos = BUFFER_LEN;
static int write_pos = 0;

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out,
                   size_t size) {
  for (size_t i = 0; i < size; i++) {
    auto sample = in[0][i] * 0.9;
    read_pos = read_pos - 1;
    if (read_pos == -1) {
      read_pos = delay_samples - 1;
    }
    auto delayed_signal = buffer[read_pos];
    buffer[write_pos] = sample + (delayed_signal * feedback);
    write_pos = (write_pos + 1) % (delay_samples);

    out[0][i] = sample + delayed_signal;
  }
}

int main(void) {

  hw.Configure();
  hw.Init();
  hw.SetAudioBlockSize(4);

  AdcChannelConfig adcConfig[2];
  adcConfig[0].InitSingle(hw.GetPin(21)); /* delay amount */
  adcConfig[1].InitSingle(hw.GetPin(20)); /* feedback */
  hw.adc.Init(adcConfig, 2);
  hw.adc.Start();

  hw.StartAudio(AudioCallback);
  hw.PrintLine("again");
  for (;;) {
    delay_samples = (int)(hw.adc.GetFloat(0) * BUFFER_LEN);
    auto new_feedback = hw.adc.GetFloat(1);
    feedback = new_feedback;
  }
}
