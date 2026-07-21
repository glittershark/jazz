#include "ui.hpp"

#include "config.hpp"
#include "led.hpp"
#include "libjazz/color.hpp"
#include "rgb_led.hpp"
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

namespace {
constexpr const value_display::CieInterp kBlueToGreen = {
    .start = color::XYZ(18, 7, 95),
    .end = color::XYZ(35, 71, 12),
};

template <DisplayableBackingValue V>
CieInterpKnob<V> knob(const char* name, fridge::ui::RgbLed rgb_led) {
  return CieInterpKnob<V>(name, {kBlueToGreen, rgb_led});
}

}  // namespace

ui::UI::UI(io::led::Controller& led, config::Config initial_config)
    : config_(initial_config),
      head{
          // D16
          .position = knob<Position>(
              "Position", RgbLed(led.B(2, 0), led.B(2, 1), led.B(2, 2))),
          // D22
          .write_amount = knob<SingleTurn>(
              "Write Amount", RgbLed(led.B(3, 0), led.B(3, 1), led.B(3, 2))),
          // D28
          .read_amount = knob<SingleTurn>(
              "Read Amount", RgbLed(led.B(4, 0), led.B(4, 1), led.B(4, 2))),
          // D34
          .erase_amount = knob<SingleTurn>(
              "Erase Amount", RgbLed(led.B(5, 0), led.B(5, 1), led.B(5, 2))),
          // D40
          .feedback =
              FeedbackKnob(RgbLed(led.B(6, 0), led.B(6, 1), led.B(6, 2)),
                           FeedbackKnob::Config{
                               .max_read_color = color::RGB(0, 255, 0),
                               .max_erase_color = color::RGB(255, 0, 0),
                           }),
      },
      lfo{
          // D5
          .range = knob<Position>(
              "Range", RgbLed(led.B(0, 3), led.B(0, 4), led.B(0, 5))),
          // D11
          .max_grain_size = knob<Size>(
              "Max Grain Size", RgbLed(led.B(1, 3), led.B(1, 4), led.B(1, 5))),
          // D17
          .min_grain_size = knob<Size>(
              "Min Grain Size", RgbLed(led.B(2, 3), led.B(2, 4), led.B(2, 5))),
          // D23
          .reverse_chance = knob<SingleTurn>(
              "Reverse Chance", RgbLed(led.B(3, 3), led.B(3, 4), led.B(3, 5))),
          // D29
          .teleport_chance = knob<SingleTurn>(
              "Teleport Chance", RgbLed(led.B(4, 3), led.B(4, 4), led.B(4, 5))),
          // D35
          .pitch_shift_chance =
              knob<SingleTurn>("Pitch Shift Chance",
                               RgbLed(led.B(5, 3), led.B(5, 4), led.B(5, 5))),
          // D41
          .low_octave_chance =
              knob<SingleTurn>("Low Octave Chance",
                               RgbLed(led.B(6, 3), led.B(6, 4), led.B(6, 5))),
          // D47
          .high_octave_chance =
              knob<SingleTurn>("High Octave Chance",
                               RgbLed(led.B(7, 3), led.B(7, 4), led.B(7, 5))),
      },
      // D4
      dry(knob<SingleTurn>("Dry",
                           RgbLed(led.B(0, 0), led.B(0, 1), led.B(0, 2)))),
      // D10
      wet(knob<SingleTurn>("Wet",
                           RgbLed(led.B(1, 0), led.B(1, 1), led.B(1, 2)))),

      // D1, D7, D13, D19, D25, D31, D37, D43
      head_select(
          std::array<RgbLed, 8U>{
              RgbLed(led.A(0, 0), led.A(0, 1), led.A(0, 2)),
              RgbLed(led.A(1, 0), led.A(1, 1), led.A(1, 2)),
              RgbLed(led.A(2, 0), led.A(2, 1), led.A(2, 2)),
              RgbLed(led.A(3, 0), led.A(3, 1), led.A(3, 2)),
              RgbLed(led.A(4, 0), led.A(4, 1), led.A(4, 2)),
              RgbLed(led.A(5, 0), led.A(5, 1), led.A(5, 2)),
              RgbLed(led.A(6, 0), led.A(6, 1), led.A(6, 2)),
              RgbLed(led.A(7, 0), led.A(7, 1), led.A(7, 2)),
          },
          // TODO(aspen): Pick a real color. Or use the color to indicate
          // something.
          color::RGB(0, 255, 0)),
      // D2, D8, D14, D20, D26, D32, D38, D44
      lfo_select(
          std::array<RgbLed, 8U>{
              RgbLed(led.A(0, 3), led.A(0, 4), led.A(0, 5)),
              RgbLed(led.A(1, 3), led.A(1, 4), led.A(1, 5)),
              RgbLed(led.A(2, 3), led.A(2, 4), led.A(2, 5)),
              RgbLed(led.A(3, 3), led.A(3, 4), led.A(3, 5)),
              RgbLed(led.A(4, 3), led.A(4, 4), led.A(4, 5)),
              RgbLed(led.A(5, 3), led.A(5, 4), led.A(5, 5)),
              RgbLed(led.A(6, 3), led.A(6, 4), led.A(6, 5)),
              RgbLed(led.A(7, 3), led.A(7, 4), led.A(7, 5)),
          },
          // TODO(aspen): Pick a real color. Or use the color to indicate
          // something (like the lfo moving?)
          color::RGB(0, 255, 0)) {
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
