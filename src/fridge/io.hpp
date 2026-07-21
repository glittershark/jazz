#ifndef IO_H_
#define IO_H_

#ifndef UNIT_TEST

#include <array>
#include <optional>

#include "callback.hpp"
#include "daisy_seed.h"
#include "led.hpp"
#include "libjazz/color.hpp"
#include "pwm.hpp"

namespace fridge::io {

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
  CB* callback_;

  // We scan through the mux one channel every 100 microseconds
  static constexpr const uint32_t kFreqUs = 100;

  void TimerCallback() {
    auto channel = address_.Channel();
    auto arg = callback_->BeforeChange(channel);
    address_.SelectChannel((channel + 1) % kNumChannels);
    callback_->AfterChange(channel, arg);
  }

 public:
  ChannelScan(const ChannelScan&) = delete;
  ChannelScan& operator=(const ChannelScan&) = delete;

  ChannelScan(Address address, CB* callback)
      : address_(std::move(address)), callback_(callback) {};

  Callback<> GetCallback() {
    return {
        .callback =
            +[](void* self) {
              static_cast<ChannelScan*>(self)->TimerCallback();
            },
        .data = this,
    };
  }
};

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

    // TODO: migrate entirely to Callback<bool>
    void OnChange(void (*callback)(void*, bool), void* data);
    void OnChange(Callback<bool> cb);
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
  Callback<int, float> on_change_;

  void AChanged(bool new_value);
  static void a_changed(void* this_, bool new_value) {
    ((QuadratureEncoder*)this_)->AChanged(new_value);
  }
  void BChanged(bool new_value);
  static void b_changed(void* this_, bool new_value) {
    ((QuadratureEncoder*)this_)->BChanged(new_value);
  }

  void Changed(int ticks);

 public:
  QuadratureEncoder(const QuadratureEncoder&) = delete;
  QuadratureEncoder& operator=(const QuadratureEncoder&) = delete;

  QuadratureEncoder(mux::GpioInMux::Channel a, mux::GpioInMux::Channel b,
                    uint32_t ticks_per_turn = 96);

  // chained constructor so we can use std::initializer_list
  QuadratureEncoder(std::array<mux::GpioInMux::Channel, 2> channels)
      : QuadratureEncoder(channels[0], channels[1]) {}

  void OnChange(Callback<int, float>);
};

class Button {
  mux::GpioInMux::Channel c_;
  Callback<bool> on_change_;

  void Changed(bool new_value) { on_change_(new_value); }

 public:
  Button(const Button&) = delete;
  Button& operator=(const Button&) = delete;

  Button(mux::GpioInMux::Channel c);

  void OnChange(Callback<bool>);
};

}  // namespace fridge::io

#endif  // UNIT_TEST

#endif  // IO_H_
