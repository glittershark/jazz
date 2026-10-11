#ifndef UNIT_TEST

#include <atomic>
#include <new>

#include "daisy_seed.h"
#include "la_sort.hpp"
#include "panel.hpp"

namespace {
daisy::DaisySeed hw;
la_sort::Effect effect;
la_sort::Parameters parameters[2];
alignas(la_sort::Panel) std::byte panel_memory[sizeof(la_sort::Panel)];
std::atomic<unsigned> active_parameters{0};
static_assert(std::atomic<unsigned>::is_always_lock_free);

void AudioCallback(daisy::AudioHandle::InputBuffer in,
                   daisy::AudioHandle::OutputBuffer out, size_t size) {
  const auto& current =
      parameters[active_parameters.load(std::memory_order_acquire)];
  for (size_t i = 0; i < size; ++i) {
    // Preserve the pedal's mono input; write the result to both outputs.
    const auto sample = jazz::audio::StereoSample::OfMono(in[0][i]);
    const auto processed = effect.ProcessSample(current, sample);
    out[0][i] = processed.left;
    out[1][i] = processed.right;
  }
}
}  // namespace

int main() {
  hw.Configure();
  hw.Init();
  hw.SetAudioSampleRate(daisy::SaiHandle::Config::SampleRate::SAI_48KHZ);
  hw.SetAudioBlockSize(4);

  la_sort::Config config;
  // Panel GPIO, timer, and LED peripherals must be initialized after hw.Init.
  auto* panel = new (panel_memory) la_sort::Panel(config);
  hw.StartAudio(AudioCallback);

  for (;;) {
    if (panel->Read(config)) {
      const unsigned next =
          1 - active_parameters.load(std::memory_order_relaxed);
      // The main loop cannot run during the audio ISR. Prepare the inactive
      // table with interrupts enabled, then publish it for the next block.
      if (parameters[next].SetConfig(config)) {
        active_parameters.store(next, std::memory_order_release);
      }
    }
    daisy::System::Delay(10);
  }
}

#endif  // UNIT_TEST
