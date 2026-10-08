#include "io.hpp"

#include "callback.hpp"

#ifndef UNIT_TEST

namespace fridge::io {
namespace mux {

/// Address

Address::Address(Pin a, Pin b, Pin c) : channel_(0) {
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

GpioInMux::GpioInMux(Pin pin) : last_value_({}), callbacks_({}) {
  pin_.Init(pin, GPIO::Mode::INPUT, GPIO::Pull::PULLUP);
}

void GpioInMux::RegisterCallback(uint8_t channel, Callback<bool> callback) {
  assert(channel < kNumChannels);
  for (auto& cb : callbacks_[channel]) {
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
    for (auto& cb : callbacks_[channel]) {
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

void GpioInMux::Channel::OnChange(void (*callback)(void*, bool), void* data) {
  mux_->RegisterCallback(channel_, {.callback = callback, .data = data});
}

void GpioInMux::Channel::OnChange(Callback<bool> cb) {
  mux_->RegisterCallback(channel_, cb);
}

}  // namespace mux

QuadratureEncoder::QuadratureEncoder(mux::GpioInMux::Channel a,
                                     mux::GpioInMux::Channel b,
                                     uint32_t ticks_per_turn)
    : a_(a), b_(b), ticks_per_turn_(ticks_per_turn) {
  a.OnChange(QuadratureEncoder::a_changed, this);
  b.OnChange(QuadratureEncoder::b_changed, this);
}

void QuadratureEncoder::AChanged(bool new_value) {
  Changed((new_value ^ b_.Read()) ? 1 : -1);
}

void QuadratureEncoder::BChanged(bool new_value) {
  Changed((new_value ^ a_.Read()) ? -1 : 1);
}

void QuadratureEncoder::Changed(int ticks) {
  on_change_(ticks,
             static_cast<float>(ticks) / static_cast<float>(ticks_per_turn_));
}

void QuadratureEncoder::OnChange(Callback<int, float> on_change) {
  on_change_ = on_change;
}

Button::Button(mux::GpioInMux::Channel c) : c_(c) {
  c_.OnChange({
      .callback =
          +[](void* self, bool high) {
            /* The buttons all short to ground when pressed, so are high by
             * default and go /low/ when pressed */
            bool pressed = !high;
            static_cast<Button*>(self)->Changed(pressed);
          },
      .data = this,
  });
}

void Button::OnChange(Callback<bool> on_change) {
  on_change_ = on_change;
}

void PushbuttonQuadratureEncoder::OnPressChanged(
    Callback<bool> on_press_changed) {
  button_.OnChange(on_press_changed);
}

}  // namespace fridge::io

#endif  // UNIT_TEST
