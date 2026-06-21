#include "ui.hpp"

#include "config.hpp"
#include "led.hpp"
#include "value_display.hpp"

#ifndef UNIT_TEST
using namespace daisy;
#endif  // UNIT_TEST

using namespace fridge;
using namespace fridge::ui;

config::Head Head::Config() const {
  return {
      .position = position.Get(),
      .write_amount = write_amount.Get(),
      .read_amount = read_amount.Get(),
      .erase_amount = erase_amount.Get(),
      .feedback = feedback.Get(),
  };
};

void Head::Select(const fridge::config::Head& head) {
  position.Set(head.position);
  write_amount.Set(head.write_amount);
  read_amount.Set(head.read_amount);
  erase_amount.Set(head.erase_amount);
  feedback.Set(head.feedback);
}

fridge::config::LFO LFO::Config() const {
  return {
      .range = range.Get(),
      .max_grain_size = max_grain_size.Get(),
      .min_grain_size = min_grain_size.Get(),
      .reverse_chance = reverse_chance.Get(),
      .teleport_chance = teleport_chance.Get(),
      .pitch_shift_chance = pitch_shift_chance.Get(),
      .low_octave_chance = low_octave_chance.Get(),
      .high_octave_chance = high_octave_chance.Get(),
  };
}

void LFO::Select(const fridge::config::LFO& lfo) {
  range.Set(lfo.range);
  max_grain_size.Set(lfo.max_grain_size);
  min_grain_size.Set(lfo.min_grain_size);
  reverse_chance.Set(lfo.reverse_chance);
  teleport_chance.Set(lfo.teleport_chance);
  pitch_shift_chance.Set(lfo.pitch_shift_chance);
  low_octave_chance.Set(lfo.low_octave_chance);
  high_octave_chance.Set(lfo.high_octave_chance);
}

const config::Config& ui::UI::Config() {
  config_.heads[selected_head] = head.Config();

  // HACK: keep the targets unchanged. Fix this once we have actual target
  // selection
  auto prev_targets = config_.lfos[selected_lfo].targets;
  config_.lfos[selected_lfo] = lfo.Config();
  config_.lfos[selected_lfo].targets = prev_targets;

  config_.dry = dry.Get();
  config_.wet = wet.Get();
  return config_;
}

constexpr const value_display::CieInterp kBlueToGreen = {
    .start = color::XYZ(18, 7, 95),
    .end = color::XYZ(35, 71, 12),
};

ui::UI::UI(io::led::Controller& led_controller, config::Config initial_config)
    : config_(initial_config),
      head{.feedback = {RgbLedValueDisplay(
               kBlueToGreen,
               RgbLed(led_controller.A(0, 2), led_controller.A(1, 2),
                      led_controller.A(3, 2)))}

      },
      wet({kBlueToGreen, RgbLed(led_controller.B(4, 5), led_controller.B(5, 5),
                                led_controller.B(6, 5))}) {
  head.Select(config_.heads[selected_head]);
  lfo.Select(config_.lfos[selected_lfo]);
  dry.Set(initial_config.dry);
  wet.Set(initial_config.wet);
}

#ifndef UNIT_TEST

TempoButton::TempoButton() : average_gap_(0) {
  const auto now = System::GetNow();

  for (auto& entry : history_) {
    entry = now;
  }

  average_gap_ = 0;
}

float TempoButton::Estimate() {
  return average_gap_ / 1000.f;
}

void TempoButton::Tick(bool state) {
  /*
   * right now, this thing makes its estimates via a rolling average. while this
   * works, the thing we /actually/ want here is a low-pass filter on dt, which
   * we could in theory achieve more effectively (for a given history length)
   * with a low-order elliptic filter. but that probably qualifies as
   * overengineering.
   */
  if (state) {
    const auto now = System::GetNow();

    // TODO(nausicaa) goddamn ring buffer, see other comment

    // 1. shift the history left one step
    for (std::size_t i = 0; i < history_.size() - 1; ++i) {
      history_[i] = history_[i + 1];
    }

    // 2. push the latest timestamp to the end of history
    history_[history_.size()] = now;

    // 3. average the delta between each pair of elements in the history
    uint64_t gap_sum = 0;
    for (std::size_t i = 0; i < history_.size() - 1; ++i) {
      gap_sum += history_[i + 1] - history_[i];
    }

    average_gap_ = gap_sum / (history_.size() - 1);
  }
}

#endif  // UNIT_TEST
