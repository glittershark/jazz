#include "daisy_seed.h"
#include "daisysp.h"
#include "hid/logger.h"
#include "libjazz/value.hpp"

using namespace daisy;
using namespace daisysp;

DaisySeed hw;

struct Update {
  enum { erase, write } kind;
  size_t finished_at;
  size_t samples;
};

#define BUFFER_LEN 44100 * 2 /* 2 seconds */
#define MAX_FADE_TIME 10000

static Value buffer[BUFFER_LEN];

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out,
                   size_t size) {
  for (size_t i = 0; i < size; i++) {
    out[0][i] = in[0][i];
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
  for (;;) {
  }
}
