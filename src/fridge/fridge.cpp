#include "per/gpio.h"
#include "per/tim.h"
#ifndef UNIT_TEST
#include "daisy_seed.h"
#include "fridge.hpp"
#include <cassert>
#include <cstdint>

daisy::DaisySeed hw;

namespace fridge {
namespace control {

/// GpioInMux

GpioInMux::GpioInMux(Pin pin, Pin address_pin_a, Pin address_pin_b,
                     Pin address_pin_c, TimerHandle::Config::Peripheral timer)
    : pin_(), address_pin_a_(), address_pin_b_(), timer_(), channel_(0) {

  pin_.Init(pin, GPIO::Mode::INPUT, GPIO::Pull::PULLUP);
  address_pin_a_.Init(address_pin_a, GPIO::Mode::OUTPUT);
  address_pin_b_.Init(address_pin_b, GPIO::Mode::OUTPUT);
  address_pin_c_.Init(address_pin_c, GPIO::Mode::OUTPUT);

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

int32_t QuadratureEncoder::Ticks() const { return ticks_; }

float QuadratureEncoder::Turns() const {
  return ((float)Ticks()) / ((float)ticks_per_turn_);
}

} // namespace control

} // namespace fridge

using namespace daisy;
using namespace daisy::seed;

int main(void) {
  hw.Init();
  hw.SetAudioBlockSize(8);
  hw.StartLog();

  auto mux_1 = fridge::control::GpioInMux(
      D3, D7, D8, D9, TimerHandle::Config::Peripheral::TIM_3);

  auto mux_2 = fridge::control::GpioInMux(
      D4, D7, D8, D9, TimerHandle::Config::Peripheral::TIM_4);

  mux_1.Start();
  mux_2.Start();

  auto enc =
      fridge::control::QuadratureEncoder(mux_1.channel(0), mux_2.channel(0));

  int32_t prev = 0;

  for (;;) {
    // TODO: this doesn't work??
    auto ticks = enc.Ticks();
    if (ticks != prev) {
      hw.PrintLine("%d ticks", ticks);
      prev = ticks;
    }
    hw.DelayMs(1);
  }
}

#endif
