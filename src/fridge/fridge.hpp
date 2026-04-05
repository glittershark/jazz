#ifndef FRIDGE_H_
#define FRIDGE_H_

#ifndef UNIT_TEST

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "color.hpp"
#include "daisy_seed.h"
#include "hid/rgb_led.h"
#include "per/gpio.h"
#include "per/tim.h"
#include "pwm.hpp"

template <typename T>
struct Callback {
  void (*callback)(void*, T);
  void* data;
};

namespace fridge {

constexpr const size_t NUM_HEADS = 8;
constexpr const size_t NUM_LFOS = 8;
constexpr const size_t MAX_TARGET_PARAMS = 64;  // ??

namespace io {

using namespace daisy;
using namespace daisy::seed;

namespace mux {

class Address {
  GPIO a_;
  GPIO b_;
  GPIO c_;
  uint8_t channel_;

 public:
  Address(const Address&) = delete;
  Address& operator=(const Address&) = delete;
  Address(Address&&) = default;
  Address& operator=(Address&&) = default;

  Address(Pin a, Pin b, Pin c);
  void SelectChannel(uint8_t channel);
  inline uint8_t Channel() const { return channel_; };
};

template <std::size_t kNumChannels, typename CB>
class ChannelScan {
  Address address_;
  TimerHandle timer_;
  CB& callback_;

  // We scan through the mux one channel every 100 microseconds
  static constexpr const uint32_t kFreqUs = 100;

  void TimerCallback() {
    auto channel = address_.Channel();
    auto arg = callback_.BeforeChange(channel);
    address_.SelectChannel((channel + 1) % kNumChannels);
    callback_.AfterChange(channel, arg);
  }

  static void timer_callback_(void* channel_scan) {
    ((ChannelScan*)channel_scan)->TimerCallback();
  };

 public:
  ChannelScan(const ChannelScan&) = delete;
  ChannelScan& operator=(const ChannelScan&) = delete;

  ChannelScan(Address address, TimerHandle::Config::Peripheral timer,
              CB& callback)
      : address_(std::move(address)), timer_(), callback_(callback) {
    TimerHandle::Config timer_config;
    timer_config.periph = timer;
    timer_config.enable_irq = true;

    timer_.Init(timer_config);
    timer_.SetCallback(ChannelScan::timer_callback_, this);
    timer_.SetPeriod((kFreqUs * timer_.GetFreq()) / 1'000'000);
  };

  TimerHandle::Result Start() { return timer_.Start(); };
};

namespace channel_scan {

template <size_t N, typename C>
static ChannelScan<N, C> make(Address address,
                              TimerHandle::Config::Peripheral timer, C& cb) {
  return ChannelScan<N, C>(std::move(address), timer, cb);
};

}  // namespace channel_scan

class GpioInMux {
 public:
  static constexpr const uint8_t kNumChannels = 8;

  GpioInMux(const GpioInMux&) = delete;
  GpioInMux& operator=(const GpioInMux&) = delete;
  GpioInMux(GpioInMux&&) = default;
  GpioInMux& operator=(GpioInMux&&) = default;

 private:
  GPIO pin_;

  std::array<bool, kNumChannels> last_value_;
  std::array<std::array<std::optional<Callback<bool>>, 8>, kNumChannels>
      callbacks_;

 public:
  GpioInMux(Pin pin);

  /** Read the current value of a channel */
  bool Read(uint8_t channel) const;

  bool BeforeChange(uint8_t channel);
  void AfterChange(uint8_t channel, bool prev);

  void RegisterCallback(uint8_t channel, Callback<bool> callback);

  class Channel {
    GpioInMux* mux_;
    uint8_t channel_;

   public:
    Channel(GpioInMux* mux, uint8_t channel) : mux_(mux), channel_(channel) {}
    bool Read() const { return mux_->Read(channel_); }

    void OnChange(void (*callback)(void*, bool), void* data);
  };

  Channel channel(uint8_t channel) { return Channel(this, channel); }
};

/**
 * Multiple GpioInMuxes that share a single set of address pins
 */
template <std::size_t N>
class MultiGpioInMux {
 public:
  static constexpr const uint8_t kNumChannels = GpioInMux::kNumChannels;

  MultiGpioInMux(const MultiGpioInMux&) = delete;
  MultiGpioInMux& operator=(const MultiGpioInMux&) = delete;
  MultiGpioInMux(MultiGpioInMux&&) = default;
  MultiGpioInMux& operator=(MultiGpioInMux&&) = default;

 private:
  std::array<GpioInMux, N> muxes_;
  std::array<bool, kNumChannels> last_value_;

 public:
  MultiGpioInMux(std::array<GpioInMux, N> muxes) : muxes_(std::move(muxes)) {};

  std::array<bool, N> BeforeChange(uint8_t channel) {
    std::array<bool, N> result;
    for (size_t i = 0; i < N; i++) {
      result[i] = muxes_[i].BeforeChange(channel);
    }
    return result;
  };

  void AfterChange(uint8_t channel, std::array<bool, N> prev) {
    for (size_t i = 0; i < N; i++) {
      muxes_[i].AfterChange(channel, prev[i]);
    }
  };

  /** Read the current value of a channel */
  std::array<bool, N> Read(uint8_t channel) const {
    std::array<bool, N> result;
    for (size_t i = 0; i < N; i++) {
      result[i] = muxes_[i].Read();
    }
    return result;
  }

  GpioInMux& operator[](size_t idx) { return muxes_[idx]; }

  GpioInMux::Channel channel(size_t mux_idx, uint8_t channel) {
    return muxes_[mux_idx].channel(channel);
  }
};

}  // namespace mux

class QuadratureEncoder {
  mux::GpioInMux::Channel a_;
  mux::GpioInMux::Channel b_;
  uint32_t ticks_per_turn_;
  volatile int32_t ticks_;

  void AChanged(bool new_value);
  static void a_changed(void* this_, bool new_value) {
    ((QuadratureEncoder*)this_)->AChanged(new_value);
  }
  void BChanged(bool new_value);
  static void b_changed(void* this_, bool new_value) {
    ((QuadratureEncoder*)this_)->BChanged(new_value);
  }

 public:
  QuadratureEncoder(const QuadratureEncoder&) = delete;
  QuadratureEncoder& operator=(const QuadratureEncoder&) = delete;

  QuadratureEncoder(mux::GpioInMux::Channel a, mux::GpioInMux::Channel b,
                    uint32_t ticks_per_turn = 1);

  int32_t Ticks() const;
  float Turns() const;
};

/**
 * RGB LED with PWM for full 0-255 color control.
 *
 * Uses a hardware timer for PWM generation. Example with TIM4:
 *
 *   RgbLed led(
 *     TimerHandle::Config::Peripheral::TIM_4,
 *     D13,  // Red   - TIM4_CH1 (PB6)
 *     D14,  // Green - TIM4_CH2 (PB7)
 *     D11   // Blue  - TIM4_CH3 (PB8)
 *   );
 *   led.Set(color::RGB(255, 128, 0));  // Orange
 *   led.Set(color::HSV(170, 255, 255)); // Cyan (via implicit conversion)
 */
class RgbLed {
  pwm::Timer timer_;
  pwm::Channel red_;
  pwm::Channel green_;
  pwm::Channel blue_;
  bool invert_;

 public:
  RgbLed(const RgbLed&) = delete;
  RgbLed& operator=(const RgbLed&) = delete;

  /**
   * Initialize a PWM RGB LED.
   *
   * @param timer Which timer peripheral to use
   * @param red_pin Pin for red LED (must be channel 1 of timer)
   * @param green_pin Pin for green LED (must be channel 2 of timer)
   * @param blue_pin Pin for blue LED (must be channel 3 of timer)
   * @param invert Set true for common-anode LEDs (inverts PWM output)
   */
  RgbLed(TimerHandle::Config::Peripheral timer, Pin red_pin, Pin green_pin,
         Pin blue_pin, bool invert = false);

  /** Set the LED color */
  void Set(color::RGB color);
};

}  // namespace io

// TODO(aspen): Remove once this is all actually used
// NOLINTBEGIN(clang-diagnostic-unused-*)
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
      : position_(0),
        write_amount_(1.),
        read_amount_(1.),
        erase_amount_(1.),
        feedback_({.kind_ = Feedback::Kind::kRead, .amount_ = 0.}) {}

  size_t& position() { return position_; }
  float& write_amount() { return write_amount_; }
  float& read_amount() { return read_amount_; }
  float& erase_amount() { return erase_amount_; }
  Feedback& feedback() { return feedback_; }
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
  Target(const Target&) = default;
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

}  // namespace config
// NOLINTEND(clang-diagnostic-unused-*)

namespace state {}  // namespace state

namespace sound {

struct Update {
  enum Kind { kErase, kWrite } kind;
  size_t finished_at;
  float value;
  size_t samples;
  Update* next_;

  class Iterator {
    const Update* cur;

   public:
    using difference_type = std::ptrdiff_t;
    using value_type = Update;

    Iterator(const Update* cur) : cur(cur) {}

    const Update& operator*() const { return *cur; }
    void operator++(int) { ++*this; };
    Iterator& operator++() {
      cur = cur->next_;
      return *this;
    };

    bool operator!=(Iterator& other) const { return other.cur != cur; }
  };
  friend Iterator;

  Iterator begin() const { return Iterator(this); }
  Iterator end() const { return Iterator(nullptr); }

  static_assert(std::input_iterator<Iterator>);
};

}  // namespace sound

}  // namespace fridge

#endif  // UNIT_TEST

#endif  // FRIDGE_H_
