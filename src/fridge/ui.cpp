#include "ui.hpp"

#include <cstdint>
#include <optional>

#include "callback.hpp"
#include "config.hpp"
#include "constants.hpp"
#include "led.hpp"
#include "libjazz/color.hpp"
#include "rgb_led.hpp"
#include "value_display.hpp"

#ifndef UNIT_TEST
#include "per/tim.h"
using namespace daisy;
#endif  // UNIT_TEST

using namespace fridge;
using namespace fridge::ui;

config::Head HeadKnobs::Config() const {
  return {
      .position = position.Get(),
      .write_amount = write_amount.Get(),
      .read_amount = read_amount.Get(),
      .erase_amount = erase_amount.Get(),
      .feedback = feedback.Get(),
  };
};

void HeadKnobs::Select(const fridge::config::Head& head) {
  position.Set(head.position);
  write_amount.Set(head.write_amount);
  read_amount.Set(head.read_amount);
  erase_amount.Set(head.erase_amount);
  feedback.Set(head.feedback);
}

fridge::config::LFO LFOKnobs::Config() const {
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

void LFOKnobs::Select(const fridge::config::LFO& lfo) {
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
  config_.heads[selected_head_] = head_knobs_.Config();

  // HACK: keep the targets unchanged. Fix this once we have actual target
  // selection
  auto prev_targets = config_.lfos[selected_lfo_].targets;
  config_.lfos[selected_lfo_] = lfo_knobs_.Config();
  config_.lfos[selected_lfo_].targets = prev_targets;

  config_.dry = dry_knob_.Get();
  config_.wet = wet_knob_.Get();
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

namespace fridge::ui {
uint32_t Now() {
#ifdef UNIT_TEST
  return 0;
#else
  return daisy::System::GetNow();
#endif
}

constexpr const radio_buttons::Config kRadioButtonConfig{
    // TODO(aspen): Pick a real color. Or use the color to indicate
    // something.
    .selected_color = color::RGB(0, 255, 0),
    .held_color = color::RGB(255, 0, 0),
};

UI::UI(io::led::Controller& led, config::Config initial_config)
    : config_(initial_config),
      head_knobs_{
          // D16
          .position = knob<OneTurnIsBufferLen>(
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
          // D46
          .pan = PanKnob(RgbLed(led.B(7, 0), led.B(7, 1), led.B(7, 2)),
                         PanKnob::Config{
                             .max_right_color = color::RGB(0, 0, 255),
                             .max_left_color = color::RGB(0, 255, 0),
                         }),
      },
      lfo_knobs_{
          // D5
          .range = knob<OneTurnIsBufferLen>(
              "Range", RgbLed(led.B(0, 3), led.B(0, 4), led.B(0, 5))),
          // D11
          .max_grain_size = knob<OneTurnIsBufferLen>(
              "Max Grain Size", RgbLed(led.B(1, 3), led.B(1, 4), led.B(1, 5))),
          // D17
          .min_grain_size = knob<OneTurnIsBufferLen>(
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
      dry_knob_(knob<SingleTurn>(
          "Dry", RgbLed(led.B(0, 0), led.B(0, 1), led.B(0, 2)))),
      // D10
      wet_knob_(knob<SingleTurn>(
          "Wet", RgbLed(led.B(1, 0), led.B(1, 1), led.B(1, 2)))),

      // D1, D7, D13, D19, D25, D31, D37, D43
      head_select_(
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
          kRadioButtonConfig),
      // D2, D8, D14, D20, D26, D32, D38, D44
      lfo_select_(
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
          kRadioButtonConfig) {
  // Load initial config into the knobs BEFORE the head_select/lfo_select
  // calls below — those fire on_change which saves-then-loads, and would
  // overwrite the initial config with the default (zero) knob values if the
  // knobs hadn't been loaded first.
  head_knobs_.Select(config_.heads[selected_head_]);
  lfo_knobs_.Select(config_.lfos[selected_lfo_]);
  dry_knob_.Set(initial_config.dry);
  wet_knob_.Set(initial_config.wet);

  head_select_.OnChange(TypedCallback<UI, uint8_t>{
      .callback = +[](UI* self, uint8_t head) { self->SelectHead(head); },
      .data = this,
  });

  lfo_select_.OnChange(TypedCallback<UI, uint8_t>{
      .callback = +[](UI* self, uint8_t lfo) { self->SelectLFO(lfo); },
      .data = this,
  });

  head_select_.Select(selected_head_);
  lfo_select_.Select(selected_lfo_);

#ifndef UNIT_TEST
  {
    daisy::TimerHandle::Config timer_config;
    timer_config.periph = daisy::TimerHandle::Config::Peripheral::TIM_4;
    timer_config.enable_irq = true;
    timer_.Init(timer_config);
    timer_.SetCallback(UI::timer_callback_, this);
    timer_.SetPeriod(timer_.GetFreq());
    timer_.Start();
  };
#endif
}

void UI::SelectHead(uint8_t head) {
  config_.heads[selected_head_] = head_knobs_.Config();
  selected_head_ = head;
  if (!AmBlinking()) {
    head_knobs_.Select(config_.heads[head]);
  }
}

void UI::SelectLFO(uint8_t lfo) {
  // HACK: Preserve the outgoing LFO's targets — LFOKnobs::Config()
  // doesn't include them (no target-selection UI yet), so writing
  // back naively would wipe modulation routes. Same trick as the
  // HACK in UI::Config above.
  auto prev_targets = config_.lfos[selected_lfo_].targets;
  config_.lfos[selected_lfo_] = lfo_knobs_.Config();
  config_.lfos[selected_lfo_].targets = prev_targets;
  selected_lfo_ = lfo;
  lfo_knobs_.Select(config_.lfos[lfo]);
}

void UI::StartBlinking() {
  StartBlinking(Now());
}
void UI::StartBlinking(uint32_t now) {
  head_knobs_.StartBlinking();
  head_knobs_.SetEnabled(false);

  head_select_.StartBlinking();
  lfo_select_.StartBlinking();

  lfo_knobs_.StartBlinking();
  lfo_knobs_.SetEnabled(false);

  wet_knob_.StartBlinking();
  wet_knob_.SetEnabled(false);
  dry_knob_.StartBlinking();
  dry_knob_.SetEnabled(false);

  blink_state_ =
      std::make_optional(BlinkState{.blink = true, .last_toggle = now});
}

void UI::StopBlinking() {
  head_knobs_.StopBlinking();
  head_knobs_.SetEnabled(true);

  head_select_.StopBlinking();
  lfo_select_.StopBlinking();

  lfo_knobs_.StopBlinking();
  lfo_knobs_.SetEnabled(true);

  wet_knob_.StopBlinking();
  wet_knob_.SetEnabled(true);
  dry_knob_.StopBlinking();
  dry_knob_.SetEnabled(true);

  blink_state_ = std::nullopt;
}

/** Tick the UI once.
 *
 * This method basically exists to step the state machine for blinking LEDs to
 * give feedback to the LFO "patch" UI one step forwards
 * */
void UI::Tick() {
  auto held_lfo = lfo_select_.held();
  // Are we currently holding down an LFO button?
  if (held_lfo.has_value()) {
    auto now = Now();
    // Are we currently blinking?
    if (blink_state_.has_value()) {
      // Yes, so toggle the blink if enough time has passed
      if (now - blink_state_->last_toggle > kBlinkFreqMs) {
        blink_state_->blink = !blink_state_->blink;
        blink_state_->last_toggle = Now();
      }
    } else if (now - held_lfo->since > kHoldDelayMs) {
      // We're not currently blinking, but we *have* been holding down the LFO
      // button long enough that we should *start* blinking
      StartBlinking(now);
    } else {
      // No blink is occurring, or should start occurring; do nothing (yet)
      return;
    }

    for (const auto& target : config_.lfos[held_lfo->which].targets) {
      if (!target.has_value()) {
        continue;
      }
      switch (target->object) {
      case fridge::config::TargetObject::kHead: {
        // Blink the target head's selector
        auto target_head_idx = target->object_idx;
        head_select_.Blink(blink_state_->blink, target_head_idx);

        // If the head is selected currently, also blink the relevant knob
        if (head_select_.selected() == target_head_idx) {
          KNOB(*this, target->parameter,
               [&](auto* knob) { knob->Blink(blink_state_->blink); });
        }
        break;
      }
      case fridge::config::TargetObject::kLFO: {
        // Blink the target lfo's selector
        auto target_lfo_idx = target->object_idx;
        lfo_select_.Blink(blink_state_->blink, target_lfo_idx);

        // If the lfo is selected currently, also blink the relevant knob
        if (lfo_select_.selected() == target_lfo_idx) {
          KNOB(*this, target->parameter,
               [&](auto* knob) { knob->Blink(blink_state_->blink); });
        }
        break;
      }
      case fridge::config::TargetObject::kMixer: {
        KNOB(*this, target->parameter,
             [&](auto* knob) { knob->Blink(blink_state_->blink); });
        break;
      }
      }
    }
  } else {
    if (blink_state_.has_value()) {
      // We're not holding down an LFO button, but we're currently blinking.
      // Stop doing that.
      StopBlinking();
    }
  }
}

config::Target UI::TargetForSelected(config::TargetParameter param) const {
  auto object = config::object_for_parameter(param);
  uint8_t object_idx = 0;
  switch (object) {
  case config::TargetObject::kHead:
    object_idx = selected_head_;
    break;
  case config::TargetObject::kLFO:
    object_idx = selected_lfo_;
    break;
  case config::TargetObject::kMixer:
    break;
  }

  return {
      .object = object,
      .parameter = param,
      .object_idx = object_idx,
  };
}

void UI::KnobPressed(config::TargetParameter param, bool pressed) {
  if (pressed) {
    auto held_lfo = lfo_select_.held();
    if (held_lfo.has_value()) {
      auto target = TargetForSelected(param);
      auto result = config_.lfos[held_lfo->which].ToggleTarget(target);

      if (result == config::ToggleResult::kToggledOff) {
        KNOB(*this, param, [&](auto* knob) { knob->BlinkOff(); });
      }
    }
  }
}

void HeadKnobs::StartBlinking() {
  EACH_HEAD_KNOB(this, [](const auto knob) { knob->BlinkOff(); });
}

void HeadKnobs::StopBlinking() {
  EACH_HEAD_KNOB(this, [](const auto knob) { knob->UpdateDisplay(); });
}

void HeadKnobs::SetEnabled(bool enabled) {
  EACH_HEAD_KNOB(this, [=](const auto knob) { knob->SetEnabled(enabled); });
}

bool HeadKnobs::Enabled() const {
  /* All knobs are enabled+disabled together, so we just return the enabled
   * status of an arbitrary knob */
  return position.Enabled();
}

void LFOKnobs::StartBlinking() {
  EACH_LFO_KNOB(this, [](const auto knob) { knob->BlinkOff(); });
}

void LFOKnobs::StopBlinking() {
  EACH_LFO_KNOB(this, [](const auto knob) { knob->UpdateDisplay(); });
}

void LFOKnobs::SetEnabled(bool enabled) {
  EACH_LFO_KNOB(this, [=](const auto knob) { knob->SetEnabled(enabled); });
}

bool LFOKnobs::Enabled() const {
  /* All knobs are enabled+disabled together, so we just return the enabled
   * status of an arbitrary knob */
  return range.Enabled();
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

}  // namespace fridge::ui
