#include <cstdint>

#include "libjazz/units.hpp"
#include "rgb_led.hpp"
#include "value_display.hpp"

#ifndef UNIT_TEST

#include <cassert>
#include <cstddef>

#include "config.hpp"
#include "daisy_seed.h"
#include "engine.hpp"
#include "libjazz/color.hpp"
#include "sound.hpp"

using namespace jazz;
using jazz::units::Samples;

using namespace fridge;
using namespace daisy;
using jazz::units::Samples;

DaisySeed hw;

constexpr const config::Config kInitialConfig{
    .heads{{
        {.position = 0,
         .write_amount = 0.0f,
         .read_amount = 1.0f,
         .erase_amount = 0.9f,
         .feedback =
             {
                 .kind = config::Feedback::Kind::kWrite,
                 .amount = 1.0f,
                 // .amount = 0.f,
             }},
        {.position = 44100,
         .write_amount = 1.f,
         .read_amount = 0.f,
         .erase_amount = 0.f,
         .feedback =
             {
                 .kind = config::Feedback::Kind::kRead,
                 // .amount = 0.f,
                 .amount = 0.4f,
             }},
    }},
    .lfos{{{
               .range = 44100 * 2,
               .max_grain_size = 19000,
               .min_grain_size = 2000,
               .reverse_chance = 0.3f,
               .teleport_chance = 0.2f,
               .pitch_shift_chance = 0.4f,
               // .pitch_shift_chance = 0.f,
               .targets{{{{
                   .object = config::TargetObject::kHead,
                   .parameter = config::TargetParameter::kPosition,
                   .object_idx = 0,
               }}}},
           },
           {
               .range = 44100 * 2,
               .max_grain_size = 19000,
               .min_grain_size = 2000,
               .reverse_chance = 0.3f,
               .teleport_chance = 0.2f,
               .pitch_shift_chance = 0.4f,
               // .pitch_shift_chance = 0.f,
               .targets{{{{
                   .object = config::TargetObject::kHead,
                   .parameter = config::TargetParameter::kPosition,
                   .object_idx = 0,
               }}}},
           }}},
    .dry = 0.6f,
    .wet = 0.4f,
};

sound::Sound* sound;
engine::Engine* engine = nullptr;

char DSY_SDRAM_BSS sound_memory[sizeof(sound::Sound)];
char engine_memory[sizeof(engine::Engine)];

constexpr const Samples<uint32_t> kAudioBlockSize = Samples(2);

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out,
                   size_t size) {
  for (size_t i = 0; i < size; ++i) {
    const auto& frame = ::engine->Tick();
    auto res = ::sound->ProcessSample(frame, in[0][i]);
    out[0][i] = res;
  }
}

[[noreturn]] void ActualFridge() {
  // XXX: construct this thing BEFORE starting the audio callback!
  ::sound = new (sound_memory) sound::Sound();
  ::engine = new (engine_memory) engine::Engine(kInitialConfig);

  hw.StartAudio(AudioCallback);
  hw.PrintLine("Now refrigerating your heads...");

  // TODO: delete
  ::engine->ui().head.feedback.EnableLogging();

  for (;;) {
  }
}

[[noreturn]] int main() {
  hw.Init();
  hw.SetAudioBlockSize(kAudioBlockSize.samples());
  hw.StartLog(false);

  ActualFridge();
}

#endif
