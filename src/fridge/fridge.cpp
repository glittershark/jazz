#include "per/gpio.h"
#include "per/tim.h"
#include <memory>
#ifndef UNIT_TEST
#include "daisy_seed.h"
#include "fridge.hpp"
#include <cassert>
#include <cstdint>

daisy::DaisySeed hw;

namespace fridge {

namespace control {

namespace mux {

/// Address

Address::Address(Pin a, Pin b, Pin c) : a_(), b_(), c_(), channel_(0) {
  a_.Init(a, GPIO::Mode::OUTPUT);
  b_.Init(b, GPIO::Mode::OUTPUT);
  c_.Init(c, GPIO::Mode::OUTPUT);
}

void Address::SelectChannel(uint8_t channel) {
  channel_ = channel;
  a_.Write(channel & 1);
  b_.Write(channel & 2);
  c_.Write(channel & 4);
}

/// GpioInMux

GpioInMux::GpioInMux(Pin pin) : pin_(), last_value_({}), callbacks_({}) {
  pin_.Init(pin, GPIO::Mode::INPUT, GPIO::Pull::PULLUP);
}

void GpioInMux::RegisterCallback(uint8_t channel, Callback<bool> callback) {
  assert(channel < kNumChannels);
  for (auto &cb : callbacks_[channel]) {
    if (!cb.has_value()) {
      cb = callback;
      return;
    }
  }
  assert(false);
}

bool GpioInMux::BeforeChange(uint8_t channel) {
  auto prev_value = last_value_[channel];
  auto new_value = pin_.Read();
  last_value_[channel] = new_value;
  return prev_value;
};

void GpioInMux::AfterChange(uint8_t channel, bool prev_value) {
  auto new_value = last_value_[channel];

  // NOTE: we call the callbacks after updating the selected channel, to keep
  // the hardware timing as close to 1 microsecond as possible
  if (new_value != prev_value) {
    for (auto &cb : callbacks_[channel]) {
      if (cb.has_value()) {
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

} // namespace mux

QuadratureEncoder::QuadratureEncoder(mux::GpioInMux::Channel a,
                                     mux::GpioInMux::Channel b,
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
  hw.StartLog(true); // wait for serial connection

  fridge::control::mux::Address addr(
      /* a = */ D17,
      /* b = */ D16,
      /* c = */ D15);

  fridge::control::mux::MultiGpioInMux<2> encoders({
      fridge::control::mux::GpioInMux(D4),
      fridge::control::mux::GpioInMux(D3),
  });

  auto scan = fridge::control::mux::channel_scan::make<8>(
      fridge::control::mux::Address(
          /* a = */ D15,
          /* b = */ D16,
          /* c = */ D17),
      TimerHandle::Config::Peripheral::TIM_3, encoders);

  auto enc1 = fridge::control::QuadratureEncoder(encoders.channel(0, 2),
                                                 encoders.channel(1, 2));
  auto enc2 = fridge::control::QuadratureEncoder(encoders.channel(0, 0),
                                                 encoders.channel(1, 0));

  int32_t prev1 = 0;
  int32_t prev2 = 0;

  scan.Start();
  for (;;) {
    auto ticks1 = enc1.Ticks();
    auto ticks2 = enc2.Ticks();

    if (ticks1 != prev1) {
      hw.PrintLine("enc 1: %d ticks", ticks1);
      prev1 = ticks1;
    }

    if (ticks2 != prev2) {
      hw.PrintLine("enc 2: %d ticks", ticks2);
      prev2 = ticks2;
    }
  }
}

#endif
