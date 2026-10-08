#include <cstdint>

#include "constants.hpp"
#include "libjazz/stereo_sample.hpp"
#include "libjazz/units.hpp"

#ifndef UNIT_TEST

#include <cassert>
#include <cstddef>

#include "config.hpp"
#include "daisy_seed.h"
#include "engine.hpp"
#include "sound.hpp"

using namespace jazz;
using jazz::units::Samples;

using namespace fridge;
using namespace daisy;

DaisySeed hw;

constexpr config::Config InitialConfig() {
  config::Config config;
  for (size_t i = 0; i < kNumHeads; ++i) {
    config.heads[i].region = static_cast<uint8_t>(i % kNumRegions);
    config.lfos[i].min_grain_size = kSampleRateHz;
    config.lfos[i].max_grain_size = kSampleRateHz;
  }
  config.dry = 0.5f;
  config.wet = 0.5f;
  return config;
}

constexpr config::Config kInitialConfig = InitialConfig();

sound::Sound* sound;
engine::Engine* engine = nullptr;

static_assert(sizeof(sound::Sound) <= 64 * 1024 * 1024);
alignas(sound::Sound) char DSY_SDRAM_BSS sound_memory[sizeof(sound::Sound)];
alignas(engine::Engine) char engine_memory[sizeof(engine::Engine)];

constexpr const Samples<uint32_t> kAudioBlockSize = Samples(2);

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out,
                   size_t size) {
  for (size_t i = 0; i < size; ++i) {
    // TODO: maybe stereo in too someday?
    auto in_sample = audio::StereoSample::OfMono(in[0][i]);

    const mod::Frame& frame = ::engine->TickSample();
    auto res = ::sound->ProcessSample(frame, in_sample);

    // TODO: fold to mono if a second cable isn't plugged in
    out[0][i] = res.left;
    out[1][i] = res.right;
  }
}

[[noreturn]] void ActualFridge() {
  // XXX: construct this thing BEFORE starting the audio callback!
  ::sound = new (sound_memory) sound::Sound();
  ::engine = new (engine_memory) engine::Engine(kInitialConfig);

  hw.StartAudio(AudioCallback);
  hw.PrintLine("Now refrigerating your heads...");

  for (;;) {
    ::engine->SyncConfig();
  }
}

[[noreturn]] int main() {
  hw.Init();
  static_assert(kSampleRateHz == 48000);
  hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
  hw.SetAudioBlockSize(kAudioBlockSize.samples());
  hw.StartLog(false);

  ActualFridge();
}

#endif
