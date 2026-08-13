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
using jazz::units::Samples;

DaisySeed hw;

constexpr const config::Config kInitialConfig{
    .heads{
        {
            {.position = 0,
             .write_amount = 0.0f,
             .read_amount = 0.0f,
             .erase_amount = 0.0f,
             .feedback =
                 {
                     .kind = config::Feedback::Kind::kRead,
                     .amount = 0.0f,
                 }},
            {.position = 0,
             .write_amount = 0.0f,
             .read_amount = 0.0f,
             .erase_amount = 0.0f,
             .feedback =
                 {
                     .kind = config::Feedback::Kind::kRead,
                     .amount = 0.0f,
                 }},
            {.position = 0,
             .write_amount = 0.0f,
             .read_amount = 0.0f,
             .erase_amount = 0.0f,
             .feedback =
                 {
                     .kind = config::Feedback::Kind::kRead,
                     .amount = 0.0f,
                 }},
            {.position = 0,
             .write_amount = 0.0f,
             .read_amount = 0.0f,
             .erase_amount = 0.0f,
             .feedback =
                 {
                     .kind = config::Feedback::Kind::kRead,
                     .amount = 0.0f,
                 }},
            {.position = 0,
             .write_amount = 0.0f,
             .read_amount = 0.0f,
             .erase_amount = 0.0f,
             .feedback =
                 {
                     .kind = config::Feedback::Kind::kRead,
                     .amount = 0.0f,
                 }},
            {.position = 0,
             .write_amount = 0.0f,
             .read_amount = 0.0f,
             .erase_amount = 0.0f,
             .feedback =
                 {
                     .kind = config::Feedback::Kind::kRead,
                     .amount = 0.0f,
                 }},
            {.position = 0,
             .write_amount = 0.0f,
             .read_amount = 0.0f,
             .erase_amount = 0.0,
             .feedback =
                 {
                     .kind = config::Feedback::Kind::kRead,
                     .amount = 0.0f,
                 }},
            {.position = 0,
             .write_amount = 0.0f,
             .read_amount = 0.0f,
             .erase_amount = 0.0,
             .feedback =
                 {
                     .kind = config::Feedback::Kind::kRead,
                     .amount = 0.0f,
                 }},
        },

    },
    .lfos{{
        {
            .range = kSampleRateHz,
            .max_grain_size = kSampleRateHz,
            .min_grain_size = kSampleRateHz,
            .reverse_chance = 0.f,
            .teleport_chance = 0.f,
            .pitch_shift_chance = 0.f,
            .targets{{{{
                .object = config::TargetObject::kHead,
                .parameter = config::TargetParameter::kPosition,
                .object_idx = 0,
            }}}},
        },
        {
            .range = kSampleRateHz,
            .max_grain_size = kSampleRateHz,
            .min_grain_size = kSampleRateHz,
            .reverse_chance = 0.f,
            .teleport_chance = 0.f,
            .pitch_shift_chance = 0.f,
            .targets{{{{
                .object = config::TargetObject::kHead,
                .parameter = config::TargetParameter::kPosition,
                .object_idx = 1,
            }}}},
        },
        {
            .range = kSampleRateHz,
            .max_grain_size = kSampleRateHz,
            .min_grain_size = kSampleRateHz,
            .reverse_chance = 0.f,
            .teleport_chance = 0.f,
            .pitch_shift_chance = 0.f,
            .targets{{{{
                .object = config::TargetObject::kHead,
                .parameter = config::TargetParameter::kPosition,
                .object_idx = 2,
            }}}},
        },
        {
            .range = kSampleRateHz,
            .max_grain_size = kSampleRateHz,
            .min_grain_size = kSampleRateHz,
            .reverse_chance = 0.f,
            .teleport_chance = 0.f,
            .pitch_shift_chance = 0.f,
            .targets{{{{
                .object = config::TargetObject::kHead,
                .parameter = config::TargetParameter::kPosition,
                .object_idx = 3,
            }}}},
        },
        {
            .range = kSampleRateHz,
            .max_grain_size = kSampleRateHz,
            .min_grain_size = kSampleRateHz,
            .reverse_chance = 0.f,
            .teleport_chance = 0.f,
            .pitch_shift_chance = 0.f,
            .targets{{{{
                .object = config::TargetObject::kHead,
                .parameter = config::TargetParameter::kPosition,
                .object_idx = 4,
            }}}},
        },
        {
            .range = kSampleRateHz,
            .max_grain_size = kSampleRateHz,
            .min_grain_size = kSampleRateHz,
            .reverse_chance = 0.f,
            .teleport_chance = 0.f,
            .pitch_shift_chance = 0.f,
            .targets{{{{
                .object = config::TargetObject::kHead,
                .parameter = config::TargetParameter::kPosition,
                .object_idx = 5,
            }}}},
        },
        {
            .range = kSampleRateHz,
            .max_grain_size = kSampleRateHz,
            .min_grain_size = kSampleRateHz,
            .reverse_chance = 0.f,
            .teleport_chance = 0.f,
            .pitch_shift_chance = 0.f,
            .targets{{{{
                .object = config::TargetObject::kHead,
                .parameter = config::TargetParameter::kPosition,
                .object_idx = 6,
            }}}},
        },
        {
            .range = kSampleRateHz,
            .max_grain_size = kSampleRateHz,
            .min_grain_size = kSampleRateHz,
            .reverse_chance = 0.f,
            .teleport_chance = 0.f,
            .pitch_shift_chance = 0.f,
            .targets{{{{
                .object = config::TargetObject::kHead,
                .parameter = config::TargetParameter::kPosition,
                .object_idx = 7,
            }}}},
        },
    }},
    .dry = 0.5f,
    .wet = 0.5f,
};

sound::Sound* sound;
engine::Engine* engine = nullptr;

char DSY_SDRAM_BSS sound_memory[sizeof(sound::Sound)];
char engine_memory[sizeof(engine::Engine)];

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

  // TODO: delete
  ::engine->ui().head_knobs.feedback.EnableLogging();
  ::engine->ui().head_knobs.position.EnableLogging();
  ::engine->ui().lfo_knobs.range.EnableLogging();
  ::engine->ui().lfo_knobs.min_grain_size.EnableLogging();
  ::engine->ui().lfo_knobs.max_grain_size.EnableLogging();

  for (;;) {
    ::engine->SyncConfig();
  }
}

[[noreturn]] int main() {
  hw.Init();
  hw.SetAudioBlockSize(kAudioBlockSize.samples());
  hw.StartLog(false);

  ActualFridge();
}

#endif
