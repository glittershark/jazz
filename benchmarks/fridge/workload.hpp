#ifndef FRIDGE_BENCHMARK_WORKLOAD_H_
#define FRIDGE_BENCHMARK_WORKLOAD_H_

#include <array>
#include <cstdint>

#include "mod.hpp"
#include "sound.hpp"

namespace fridge::benchmark {

constexpr size_t kBlockSize = 2;
constexpr std::array<const char*, 7> kScenarios{
    "idle",          "ten_heads",  "colliding_heads", "grain_storm",
    "control_churn", "all_routes", "lfo_feedback"};

// Fixed seeds and bounded input make host and board workloads reproducible.
class Workload {
 public:
  explicit Workload(size_t scenario) : scenario_(scenario) {
    for (auto& region : config_.regions) {
      region.range = 32768;
    }
    for (size_t i = 0; i < kNumHeads; ++i) {
      auto& head = config_.heads[i];
      head.region = scenario == 2 ? 0 : i % kNumRegions;
      head.position = scenario == 2 ? 0 : i * 1024;
      head.read_amount = scenario == 0 ? 0 : 0.05f;
      head.write_amount = scenario == 0 ? 0 : 0.01f;
      head.erase_amount = scenario == 0 ? 1 : 0.995f;
      auto& lfo = config_.lfos[i];
      lfo.range = 1024;
      lfo.min_grain_size = scenario >= 3 ? 1 : 4096;
      lfo.max_grain_size = scenario >= 3 ? 7 : 4096;
      lfo.reverse_chance = scenario >= 3 ? 0.5f : 0;
      lfo.teleport_chance = scenario >= 3 ? 0.5f : 0;
      lfo.pitch_shift_chance = scenario >= 3 ? 0.5f : 0;
      lfo.low_octave_chance = 0.5f;
      lfo.high_octave_chance = 0.5f;
      for (size_t p = 0; p < kMaxTargetParams; ++p) {
        lfo.targets[p] = config::Target{
            .object = config::TargetObject::kHead,
            .parameter = config::TargetParameter::kPosition,
            .object_idx = static_cast<uint8_t>((i + p) % kNumHeads)};
      }
      if (scenario == 6) {
        constexpr std::array parameters{
            config::TargetParameter::kRange,
            config::TargetParameter::kMinGrainSize,
            config::TargetParameter::kMaxGrainSize,
            config::TargetParameter::kReverseChance};
        for (size_t p = 0; p < parameters.size(); ++p) {
          lfo.targets[p + 4] = config::Target{
              .object = config::TargetObject::kLFO,
              .parameter = parameters[p],
              .object_idx = static_cast<uint8_t>((i + 1) % kNumLfos)};
        }
      }
    }
    if (scenario >= 5) {
      config_.routing = config::Routing::kAssignable;
    }
    modulator_.Reset(config_);
  }

  jazz::audio::StereoSample Tick(sound::Sound& sound) {
    if (scenario_ == 4 && sample_ % 256 == 0) {
      const size_t region = sample_ / 256 % kNumRegions;
      config_.ResizeRegion(region, 16384 + (sample_ / 256 % 2) * 16384);
      config_.AssignRegion(sample_ / 256 % kNumHeads, region);
      modulator_.SetConfig(config_);
    }
    const float input = (static_cast<int>(sample_++ % 127) - 63) / 256.0f;
    return sound.ProcessSample(modulator_.TickSample(),
                               jazz::audio::StereoSample::OfMono(input));
  }

 private:
  size_t scenario_;
  uint32_t sample_ = 0;
  config::Config config_{};
  mod::Modulator modulator_{1234};
};
}  // namespace fridge::benchmark
#endif
