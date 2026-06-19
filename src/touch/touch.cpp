#include <cstddef>

#ifndef UNIT_TEST
#include "daisy_seed.h"
#include "ringbuffer.h"

using namespace daisy;

Oscillator osc;
DaisySeed hw;
#endif

static constexpr const auto SECOND = 48000;
static constexpr const auto SAMPLE_RATE =
    SaiHandle::Config::SampleRate::SAI_48KHZ;

static RingBuffer<float, SECOND / 1000> buffer;

enum class Feature { Nothing, Peak, Trough };

static Feature update_buffer(const float sample) {
  buffer.Overwrite(sample);
  float first = buffer.
}

#ifndef UNIT_TEST
void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out,
                   size_t size) {
  for (size_t i = 0; i < size; i++) {
    auto sample = in[0][i] / 2.0;
    auto delayed_signal = buffer[buffer_pos];
    buffer[buffer_pos] = sample + (delayed_signal * FEEDBACK);
    buffer_pos = (buffer_pos + 1) % (delay_samples - 1);

    out[0][i] = sample + delayed_signal;
  }
}
#endif

#ifndef UNIT_TEST
int main(void) {
  hw.Init();
  hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
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
#endif
