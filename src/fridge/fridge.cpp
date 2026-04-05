#include "hid/rgb_led.h"
#include "per/gpio.h"
#include "per/tim.h"
#include <memory>
#ifndef UNIT_TEST
#include "daisy_seed.h"
#include "fridge.hpp"
#include <cassert>
#include <cstdint>

namespace fridge {
namespace io {
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

/// RgbLed

RgbLed::RgbLed(TimerHandle::Config::Peripheral timer, Pin red_pin,
               Pin green_pin, Pin blue_pin, bool invert)
    : timer_(timer), red_(timer_.InitChannel(1, red_pin)),
      green_(timer_.InitChannel(2, green_pin)),
      blue_(timer_.InitChannel(3, blue_pin)), invert_(invert) {}

void RgbLed::Set(color::RGB c) {
  if (invert_) {
    red_.Set(255 - c.red);
    green_.Set(255 - c.green);
    blue_.Set(255 - c.blue);
  } else {
    red_.Set(c.red);
    green_.Set(c.green);
    blue_.Set(c.blue);
  }
}

} // namespace io

} // namespace fridge

using namespace daisy;
using namespace daisy::seed;

DaisySeed hw;

int main(void) {
  hw.Init();
  hw.SetAudioBlockSize(8);
  hw.StartLog(true); // wait for serial connection

  hw.PrintLine("Hello");

  fridge::io::mux::MultiGpioInMux<2> encoders({
      fridge::io::mux::GpioInMux(D4),
      fridge::io::mux::GpioInMux(D3),
  });

  auto scan = fridge::io::mux::channel_scan::make<8>(
      fridge::io::mux::Address(
          /* a = */ D15,
          /* b = */ D16,
          /* c = */ D17),
      TimerHandle::Config::Peripheral::TIM_3, encoders);

  auto enc1 = fridge::io::QuadratureEncoder(encoders.channel(0, 2),
                                            encoders.channel(1, 2));
  auto enc2 = fridge::io::QuadratureEncoder(encoders.channel(0, 0),
                                            encoders.channel(1, 0));

  fridge::io::mux::Address led_address(D9, D10, D11);
  led_address.SelectChannel(2);

  // RGB LED on TIM4 (common anode, so inverted)
  // D14 (PB7) = TIM4_CH2 = Red
  // D13 (PB6) = TIM4_CH1 = Green
  // D12 (PB9) = TIM4_CH4 = Blue
  pwm::Timer pwm(TimerHandle::Config::Peripheral::TIM_4);
  auto red = pwm.InitChannel(/* channel = */ 2, /* pin = */ D14);
  auto green = pwm.InitChannel(/* channel = */ 1, /* pin = */ D13);
  auto blue = pwm.InitChannel(/* channel = */ 4, /* pin = */ D12);

  scan.Start();

  // Debug: try fixed red first (try both polarities)
  hw.PrintLine("Testing red=255 (ON if common cathode)");
  red.Set(255);
  green.Set(0);
  blue.Set(0);

  System::Delay(2000);

  hw.PrintLine("Now cycling hue with encoder...");

  int32_t prev_ticks = 0;

  for (;;) {
    int32_t ticks = enc1.Ticks();

    if (ticks != prev_ticks) {
      uint8_t hue = (uint8_t)(ticks & 0xFF);
      fridge::color::RGB rgb = fridge::color::HSV(hue, 255, 255);

      hw.PrintLine("hue=%d -> R=%d G=%d B=%d", hue, rgb.red, rgb.green,
                   rgb.blue);

      // Try direct (common cathode) first
      red.Set(rgb.red);
      green.Set(rgb.green);
      blue.Set(rgb.blue);

      prev_ticks = ticks;
    }
  }
}

#endif
