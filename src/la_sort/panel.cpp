#include "panel.hpp"

#include <algorithm>
#include <cmath>

#ifndef UNIT_TEST
#include "../fridge/rgb_led.hpp"
#include "../fridge/value_display.hpp"
#endif

namespace la_sort {
namespace {
bool ValidKnobUpdate(const Config& config, size_t channel);
}

bool ApplyKnobTicks(Config& config, size_t channel, int ticks) {
  if (!ValidKnobUpdate(config, channel)) {
    return false;
  }
  if (channel == kLengthChannel) {
    const size_t next = static_cast<size_t>(
        std::clamp(static_cast<int64_t>(config.length) + ticks, int64_t{1},
                   static_cast<int64_t>(kMaxLength)));
    const bool changed = next != config.length;
    config.length = next;
    return changed;
  }
  const auto& knob = kKnobs[channel];
  float& value = config.*knob.parameter;
  if (knob.parameter == &Config::lfo_rate) {
    const double range = std::log(double(knob.maximum) / knob.minimum);
    const double position =
        std::clamp(std::log(double(value) / knob.minimum) / range +
                       double(ticks) / kTicksPerTurn,
                   0.0, 1.0);
    const float next =
        std::clamp(float(knob.minimum * std::exp(position * range)),
                   knob.minimum, knob.maximum);
    const bool changed = next != value;
    value = next;
    return changed;
  }
  const float increment = static_cast<float>(ticks) / kTicksPerTurn;
  const float next =
      std::clamp(value + increment * (knob.maximum - knob.minimum),
                 knob.minimum, knob.maximum);
  const bool changed = next != value;
  value = next;
  return changed;
}

#ifndef UNIT_TEST
Panel::Panel(const Config& initial)
    : mux_({fridge::io::mux::GpioInMux(daisy::seed::D5),
            fridge::io::mux::GpioInMux(daisy::seed::D6)}),
      scan_({daisy::seed::D0, daisy::seed::D1, daisy::seed::D2}, &mux_),
      encoders_{{{mux_.channel(0, 0), mux_.channel(1, 0), kTicksPerTurn},
                 {mux_.channel(0, 1), mux_.channel(1, 1), kTicksPerTurn},
                 {mux_.channel(0, 2), mux_.channel(1, 2), kTicksPerTurn},
                 {mux_.channel(0, 3), mux_.channel(1, 3), kTicksPerTurn},
                 {mux_.channel(0, 4), mux_.channel(1, 4), kTicksPerTurn},
                 {mux_.channel(0, 5), mux_.channel(1, 5), kTicksPerTurn},
                 {mux_.channel(0, 6), mux_.channel(1, 6), kTicksPerTurn}}},
      leds_({.interrupt = daisy::seed::D13, .shutdown = daisy::seed::D14}) {
  static_assert(std::atomic<int>::is_always_lock_free);
  const auto scan_callback = scan_.GetCallback();

  // Prime both quadrature inputs before registering turn handlers. Otherwise
  // the initial pull-up levels could be mistaken for actual encoder movement.
  for (size_t channel = 0; channel < 8; ++channel) {
    daisy::System::DelayUs(100);
    scan_callback();
  }

  for (size_t channel = 0; channel < encoders_.size(); ++channel) {
    encoders_[channel].OnChange({
        .callback =
            +[](void* context, int ticks, float /* turns */) {
              static_cast<std::atomic<int>*>(context)->fetch_add(
                  ticks, std::memory_order_relaxed);
            },
        .data = &pending_ticks_[channel],
    });
    Display(channel, initial);
  }

  daisy::TimerHandle::Config timer_config;
  timer_config.periph = daisy::TimerHandle::Config::Peripheral::TIM_3;
  timer_config.enable_irq = true;
  timer_.Init(timer_config);
  timer_.SetCallback(scan_callback.callback, scan_callback.data);
  // Same mux cadence as Fridge: one channel every 100 us.
  timer_.SetPeriod(timer_.GetFreq() / 10000);
  timer_.Start();
}

bool Panel::Read(Config& config) {
  bool changed = false;
  for (size_t channel = 0; channel < pending_ticks_.size(); ++channel) {
    const int ticks =
        pending_ticks_[channel].exchange(0, std::memory_order_relaxed);
    if (ApplyKnobTicks(config, channel, ticks)) {
      Display(channel, config);
      changed = true;
    }
  }
  return changed;
}

void Panel::Display(size_t channel, const Config& config) {
  // Use Fridge's existing knob LED locations and blue-to-green value scale.
  constexpr fridge::ui::value_display::CieInterp colors{
      .start = jazz::color::XYZ(18, 7, 95),
      .end = jazz::color::XYZ(35, 71, 12),
  };
  float normalized;
  uint8_t column;
  if (channel == kLengthChannel) {
    normalized = float(config.length - 1) / float(kMaxLength - 1);
    column = static_cast<uint8_t>(channel);
  } else {
    const auto& knob = kKnobs[channel];
    const float value = config.*knob.parameter;
    normalized = knob.parameter == &Config::lfo_rate
                     ? std::log(value / knob.minimum) /
                           std::log(knob.maximum / knob.minimum)
                     : (value - knob.minimum) / (knob.maximum - knob.minimum);
    column = knob.led_column;
  }
  const auto level = static_cast<uint8_t>(normalized * 255.0f);
  fridge::ui::RgbLed led(leds_.B(column, 0), leds_.B(column, 1),
                         leds_.B(column, 2));
  led.SetColor(colors(level));
  led.SetOn(true);
}
#endif

// ---------------------------------------------------------------------------
// Validation
// ---------------------------------------------------------------------------

namespace {
bool ValidKnobUpdate(const Config& config, size_t channel) {
  return channel < kKnobs.size() && IsValid(config);
}
}  // namespace
}  // namespace la_sort
