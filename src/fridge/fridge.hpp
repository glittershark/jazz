#ifndef FRIDGE_H_
#define FRIDGE_H_

#include <cassert>
#ifndef UNIT_TEST

#include "daisy_seed.h"
#include "per/tim.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

struct Callback {
  void (*callback)(void *, bool);
  void *data;
};

namespace fridge {

constexpr const size_t NUM_HEADS = 8;
constexpr const size_t NUM_LFOS = 8;
constexpr const size_t MAX_TARGET_PARAMS = 64; // ??

namespace control {

using namespace daisy;
using namespace daisy::seed;

class GpioInMux {
  GPIO pin_;
  GPIO address_pin_a_;
  GPIO address_pin_b_;
  GPIO address_pin_c_;
  TimerHandle timer_;
  uint8_t channel_;

  // We scan through the mux one channel every us
  static constexpr const uint32_t kGpioMuxFreqUs = 1;

  static constexpr const uint8_t kNumChannels = 8;

  std::array<bool, kNumChannels> last_value_;
  std::array<std::array<std::optional<Callback>, 8>, kNumChannels> callbacks_;

  void SelectChannel(uint8_t channel);
  void TimerCallback();

  static void timer_callback_(void *);

public:
  GpioInMux(GPIO pin, GPIO address_pin_a, GPIO address_pin_b,
            GPIO address_pin_c, TimerHandle::Config::Peripheral timer);

  void Start();

  /** Read the current value of a channel */
  bool Read(uint8_t channel) const;

  void RegisterCallback(uint8_t channel, Callback callback);

  class Channel {
    GpioInMux *mux_;
    uint8_t channel_;

  public:
    Channel(GpioInMux *mux, uint8_t channel) : mux_(mux), channel_(channel) {}
    bool Read() const { return mux_->Read(channel_); }

    void OnChange(void (*callback)(void *, bool), void *data);
  };

  Channel channel(uint8_t channel) { return Channel(this, channel); }
};

class QuadratureEncoder {
  GpioInMux::Channel a_;
  GpioInMux::Channel b_;
  uint32_t ticks_per_turn_;
  int32_t ticks_;

  void AChanged(bool new_value);
  static void a_changed(void *this_, bool new_value) {
    ((QuadratureEncoder *)this_)->AChanged(new_value);
  }
  void BChanged(bool new_value);
  static void b_changed(void *this_, bool new_value) {
    ((QuadratureEncoder *)this_)->BChanged(new_value);
  }

public:
  QuadratureEncoder(GpioInMux::Channel a, GpioInMux::Channel b,
                    uint32_t ticks_per_turn = 1);

  int32_t Ticks() const;
  float Turns() const;
};

// TODO: knobs and buttons and things
} // namespace control

namespace config {

struct Feedback {
  enum class Kind { kRead, kErase } kind_;
  float amount_;
};

class Head {
  // Position relative to the global clock
  size_t position_;
  float write_amount_;
  float read_amount_;
  float erase_amount_;
  Feedback feedback_;

public:
  Head()
      : position_(0), write_amount_(1.), read_amount_(1.), erase_amount_(1.),
        feedback_({.kind_ = Feedback::Kind::kRead, .amount_ = 0.}) {}

  size_t &position() { return position_; }
  float &write_amount() { return write_amount_; }
  float &read_amount() { return read_amount_; }
  float &erase_amount() { return erase_amount_; }
  Feedback &feedback() { return feedback_; }
};

enum class TargetParameter {
  Position,
  WriteAmount,
  ReadAmount,
  EraseAmount,
  Feedback,
};

class Target {
  TargetParameter parameter_;
  size_t head_idx_;

public:
  Target(const Target &) = default;
};

class LFO {
  // The duration over which the LFO oscillates
  size_t range_;
  size_t max_grain_size_;
  size_t min_grain_size_;
  // Value between 0 and 1
  float reverse_chance_;
  // Value between 0 and 1
  float teleport_chance_;
  // Value between 0 and 1
  float pitch_shift_chance_;
  // Value between 0 and 1
  float low_octave_chance_;
  // Value between 0 and 1
  float high_octave_chance_;

  std::array<std::optional<Target>, MAX_TARGET_PARAMS> targets_;
};

class Config {
  std::array<Head, NUM_HEADS> heads_;
  std::array<LFO, NUM_LFOS> lfos_;
  float dry_;
  float wet_;
};

} // namespace config

namespace state {} // namespace state

} // namespace fridge

#endif

#endif // FRIDGE_H_
