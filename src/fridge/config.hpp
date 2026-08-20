#ifndef CONFIG_H_
#define CONFIG_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "constants.hpp"
#include "libjazz/pan.hpp"
#include "libjazz/stereo_sample.hpp"

namespace fridge::config {

namespace {
using jazz::audio::Pan;
using jazz::audio::StereoSample;

}  // namespace

struct Feedback {
  enum class Kind { kRead, kErase };

  Kind kind = Kind::kRead;
  float amount = 0.0f;

  bool operator==(const Feedback& rhs) const {
    // now, now, we shouldn't directly compare floats... but this is mostly just
    // for testing, so
    return kind == rhs.kind && amount == rhs.amount;
  }
};

// Default heads are inert: they neither write, read, nor erase. Only heads
// the config explicitly turns on touch the tape.
struct Head {
  size_t position = 0;
  float write_amount = 0.0f;
  float read_amount = 0.0f;
  float erase_amount = 1.0f;
  Feedback feedback{};
  Pan pan = jazz::audio::Pan::Center();

  bool operator==(const Head& rhs) const = default;

  StereoSample WriteAmount() const {
    return StereoSample::OfMono(write_amount).Panned(pan);
  }

  StereoSample ReadAmount() const {
    return StereoSample::OfMono(read_amount).Panned(pan);
  }

  StereoSample EraseAmount() const {
    return StereoSample::OfMono(erase_amount).Panned(pan);
  }
};

enum class TargetObject : uint8_t { kHead, kLFO, kMixer };

enum class TargetParameter : uint8_t {
  kPosition,
  kWriteAmount,
  kReadAmount,
  kEraseAmount,
  kFeedbackAmount,
  kPan,
  kRange,
  kMaxGrainSize,
  kMinGrainSize,
  kReverseChance,
  kTeleportChance,
  kPitchShiftChance,
  kLowOctaveChance,
  kHighOctaveChance,
  kDry,
  kWet,
};

TargetObject object_for_parameter(TargetParameter param);

enum class ToggleResult { kToggledOn, kToggledOff, kNothingHappened };

// Packed to 3 bytes so a full target list stays small; an
// std::optional<Target> is 4 bytes.
struct Target {
  TargetObject object = TargetObject::kHead;
  TargetParameter parameter = TargetParameter::kPosition;
  uint8_t object_idx = 0;

  bool operator==(const Target& rhs) const = default;
};

struct LFO {
  size_t range = 0;
  size_t max_grain_size = 1;
  size_t min_grain_size = 1;
  float reverse_chance = 0.0f;
  float teleport_chance = 0.0f;
  float pitch_shift_chance = 0.0f;
  float low_octave_chance = 0.0f;
  float high_octave_chance = 0.0f;
  std::array<std::optional<Target>, kMaxTargetParams> targets{};

  bool operator==(const LFO& rhs) const = default;

  ToggleResult ToggleTarget(const Target& target);
};

struct Config {
  std::array<Head, kNumHeads> heads{};
  std::array<LFO, kNumLfos> lfos{};
  float dry = 1.0f;
  float wet = 1.0f;

  bool operator==(const Config& rhs) const = default;
};

/**
 * The one live copy of the config, plus whether the audio engine has caught up
 * with it. Written from the mux timer interrupt, read from the main loop.
 */
class ConfigStore {
  Config config_;
  volatile bool dirty_ = false;

 public:
  ConfigStore() = default;
  explicit ConfigStore(const Config& config) : config_(config) {}

  const Config& Read() const { return config_; }

  Config& Write() {
    dirty_ = true;
    return config_;
  }

  bool dirty() const { return dirty_; }
  void MarkClean() { dirty_ = false; }
};

}  // namespace fridge::config
// NOLINTEND(clang-diagnostic-unused-*)

#endif  // CONFIG_H_
