#ifndef UNIT_TEST
#include "fridge.hpp"
#include <cassert>
#include <cstdint>

namespace fridge {
namespace control {

/// GpioInMux

GpioInMux::GpioInMux(GPIO pin, GPIO address_pin_a, GPIO address_pin_b,
                     GPIO address_pin_c, TimerHandle::Config::Peripheral timer)
    : pin_(pin), address_pin_a_(address_pin_a), address_pin_b_(address_pin_b),
      address_pin_c_(address_pin_c), timer_(), channel_(0) {

  TimerHandle::Config timer_config;
  timer_config.periph = timer;
  timer_config.enable_irq = true;
  timer_.Init(timer_config);
  timer_.SetCallback(GpioInMux::timer_callback_, this);

  timer_.SetPeriod((kGpioMuxFreqUs * timer_.GetFreq()) / 1'000'000);
}

void GpioInMux::Start() { timer_.Start(); }

void GpioInMux::SelectChannel(uint8_t channel) {
  address_pin_a_.Write(channel & 1);
  address_pin_b_.Write(channel & 2);
  address_pin_c_.Write(channel & 4);
}

void GpioInMux::RegisterCallback(uint8_t channel, Callback callback) {
  assert(channel < kNumChannels);
  for (auto &cb : callbacks_[channel]) {
    if (!cb) {
      cb = callback;
      return;
    }
  }
  assert(false);
}

void GpioInMux::timer_callback_(void *gpio_mux) {
  ((GpioInMux *)gpio_mux)->TimerCallback();
}
void GpioInMux::TimerCallback() {
  auto prev_value = last_value_[channel_];
  auto new_value = pin_.Read();
  auto channel = channel_;
  last_value_[channel] = new_value;
  channel_ = (channel_ + 1) % kNumChannels;
  SelectChannel(channel_);

  // NOTE: we call the callbacks after updating the selected channel, to keep
  // the hardware timing as close to 1 microsecond as possible
  if (new_value != prev_value) {
    for (auto &&cb : callbacks_[channel]) {
      if (cb) {
        cb->callback(cb->data, new_value);
      }
    }
  }
}

bool GpioInMux::Read(uint8_t channel) const {
  assert(channel < kNumChannels);
  return last_value_[channel];
}

void GpioInMux::Channel::OnChange(void (*callback)(void *, bool), void *data) {
  mux_->RegisterCallback(channel_, {callback, data});
}

/// RainbowLedFeedbackEncoder

QuadratureEncoder::QuadratureEncoder(GpioInMux::Channel a, GpioInMux::Channel b,
                                     uint32_t ticks_per_turn)
    : a_(a), b_(b), ticks_per_turn_(ticks_per_turn), ticks_(0) {
  a.OnChange(QuadratureEncoder::a_changed, this);
  b.OnChange(QuadratureEncoder::b_changed, this);
}

void QuadratureEncoder::AChanged(bool new_value) {
  ticks_ += ((new_value ^ b_.Read()) ? 1 : -1);
}
void QuadratureEncoder::BChanged(bool new_value) {
  ticks_ += ((new_value ^ a_.Read()) ? -1 : 1);
}

uint32_t QuadratureEncoder::Ticks() const { return ticks_; }

float QuadratureEncoder::Turns() const {
  return ((float)Ticks()) / ((float)ticks_per_turn_);
}

} // namespace control

} // namespace fridge

#include "daisy_seed.h"

daisy::DaisySeed hw;

int main(void) {
  hw.Init();
  hw.SetAudioBlockSize(8);
  hw.StartLog();
}

#endif
