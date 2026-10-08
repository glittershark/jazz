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

void HeadKnobs::WriteInto(fridge::config::Head& head) const {
  head.write_amount = write_amount.Get();
  head.read_amount = read_amount.Get();
  head.erase_amount = erase_amount.Get();
  head.feedback = feedback.Get();
  head.pan = pan.Get();
}

void HeadKnobs::Select(const fridge::config::Head& head, size_t range) {
  position.Set(static_cast<float>(head.position) / range);
  write_amount.Set(head.write_amount);
  read_amount.Set(head.read_amount);
  erase_amount.Set(head.erase_amount);
  feedback.Set(head.feedback);
  pan.Set(head.pan);
}

void LFOKnobs::WriteInto(fridge::config::LFO& lfo) const {
  lfo.max_grain_size = max_grain_size.Get();
  lfo.min_grain_size = min_grain_size.Get();
  lfo.reverse_chance = reverse_chance.Get();
  lfo.teleport_chance = teleport_chance.Get();
  lfo.pitch_shift_chance = pitch_shift_chance.Get();
  lfo.low_octave_chance = low_octave_chance.Get();
  lfo.high_octave_chance = high_octave_chance.Get();
}

void LFOKnobs::Select(const fridge::config::LFO& lfo) {
  max_grain_size.Set(lfo.max_grain_size);
  min_grain_size.Set(lfo.min_grain_size);
  reverse_chance.Set(lfo.reverse_chance);
  teleport_chance.Set(lfo.teleport_chance);
  pitch_shift_chance.Set(lfo.pitch_shift_chance);
  low_octave_chance.Set(lfo.low_octave_chance);
  high_octave_chance.Set(lfo.high_octave_chance);
}

void ui::UI::WriteHead() {
  head_knobs_.WriteInto(config_->Write().heads[selected_head_]);
}

void ui::UI::WriteLFO() {
  lfo_knobs_.WriteInto(config_->Write().lfos[selected_head_]);
}

void ui::UI::WriteMixer() {
  config::Config& config = config_->Write();
  config.dry = dry_knob_.Get();
  config.wet = wet_knob_.Get();
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

UI::UI(io::led::Controller& led, config::ConfigStore* config)
    : config_(config),
      head_knobs_{
          // D16
          .position = knob<WrappedTurns>(
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
      // D5: region range, shared by every head assigned to that region.
      range_knob_(knob<OneTurnIsBufferLen>(
          "Range", RgbLed(led.B(0, 3), led.B(0, 4), led.B(0, 5)))),
      // D4
      dry_knob_(knob<SingleTurn>(
          "Dry", RgbLed(led.B(0, 0), led.B(0, 1), led.B(0, 2)))),
      // D10
      wet_knob_(knob<SingleTurn>(
          "Wet", RgbLed(led.B(1, 0), led.B(1, 1), led.B(1, 2)))),

      // D1, D7, D13, D19, D25, D31, D37, D43
      head_select_(
          std::array<RgbLed, kNumHeads>{
              RgbLed(led.A(0, 0), led.A(0, 1), led.A(0, 2)),
              RgbLed(led.A(1, 0), led.A(1, 1), led.A(1, 2)),
              RgbLed(led.A(2, 0), led.A(2, 1), led.A(2, 2)),
              RgbLed(led.A(3, 0), led.A(3, 1), led.A(3, 2)),
              RgbLed(led.A(4, 0), led.A(4, 1), led.A(4, 2)),
              RgbLed(led.A(5, 0), led.A(5, 1), led.A(5, 2)),
              RgbLed(led.A(6, 0), led.A(6, 1), led.A(6, 2)),
              RgbLed(led.A(7, 0), led.A(7, 1), led.A(7, 2)),
              RgbLed(led.A(0, 3), led.A(0, 4), led.A(0, 5)),
              RgbLed(led.A(1, 3), led.A(1, 4), led.A(1, 5)),
          },
          kRadioButtonConfig),
      // D14, D20, D26, D32, D38, D44
      region_select_(
          std::array<RgbLed, kNumRegions>{
              RgbLed(led.A(2, 3), led.A(2, 4), led.A(2, 5)),
              RgbLed(led.A(3, 3), led.A(3, 4), led.A(3, 5)),
              RgbLed(led.A(4, 3), led.A(4, 4), led.A(4, 5)),
              RgbLed(led.A(5, 3), led.A(5, 4), led.A(5, 5)),
              RgbLed(led.A(6, 3), led.A(6, 4), led.A(6, 5)),
              RgbLed(led.A(7, 3), led.A(7, 4), led.A(7, 5)),
          },
          kRadioButtonConfig) {
  LoadSelection();
  dry_knob_.Set(config_->Read().dry);
  wet_knob_.Set(config_->Read().wet);

  head_knobs_.OnChange(Callback<>{
      .callback = +[](void* self) { static_cast<UI*>(self)->WriteHead(); },
      .data = this,
  });

  lfo_knobs_.OnChange(Callback<>{
      .callback = +[](void* self) { static_cast<UI*>(self)->WriteLFO(); },
      .data = this,
  });

  head_knobs_.position.OnChange(Callback<>{
      .callback = +[](void* self) { static_cast<UI*>(self)->WritePosition(); },
      .data = this,
  });
  range_knob_.OnChange(Callback<>{
      .callback = +[](void* self) { static_cast<UI*>(self)->WriteRange(); },
      .data = this,
  });

  auto write_mixer = Callback<>{
      .callback = +[](void* self) { static_cast<UI*>(self)->WriteMixer(); },
      .data = this,
  };
  dry_knob_.OnChange(write_mixer);
  wet_knob_.OnChange(write_mixer);

  head_select_.OnChange(Callback<uint8_t>{
      .callback = +[](void* self, uint8_t head) { static_cast<UI*>(self)->SelectHead(head); },
      .data = this,
  });

  region_select_.OnChange(Callback<uint8_t>{
      .callback = +[](void* self, uint8_t region) { static_cast<UI*>(self)->AssignRegion(region); },
      .data = this,
  });

  head_select_.Select(selected_head_, /*instantaneous=*/true);

#ifndef UNIT_TEST
  {
    daisy::TimerHandle::Config timer_config;
    timer_config.periph = daisy::TimerHandle::Config::Peripheral::TIM_4;
    timer_config.enable_irq = true;
    timer_.Init(timer_config);
    timer_.SetCallback(
        +[](void* self) { static_cast<UI*>(self)->Tick(); }, this);
    timer_.SetPeriod(timer_.GetFreq() / 20);
    timer_.Start();
  };
#endif
}

void UI::LoadSelection() {
  const auto& config = config_->Read();
  const auto& head = config.heads[selected_head_];
  const size_t range = config.regions[head.region].range;
  head_knobs_.Select(head, range);
  lfo_knobs_.Select(config.lfos[selected_head_]);
  range_knob_.Set(range);
  region_select_.Select(head.region, /*instantaneous=*/true, /*notify=*/false);
  range_rejected_at_.reset();
  range_flash_on_.reset();
}

void UI::SelectHead(uint8_t head) {
  if (head >= kNumHeads) {
    return;
  }
  selected_head_ = head;
  head_select_.Select(head, /*instantaneous=*/true, /*notify=*/false);
  LoadSelection();
}

void UI::AssignRegion(uint8_t region) {
  if (region >= kNumRegions) {
    return;
  }
  config_->Write().AssignRegion(selected_head_, region);
  LoadSelection();
}

void UI::WritePosition() {
  auto& config = config_->Write();
  auto& head = config.heads[selected_head_];
  const size_t range = config.regions[head.region].range;
  head.position = static_cast<size_t>(
                      static_cast<float>(head_knobs_.position.Get()) * range) %
                  range;
}

void UI::WriteRange() {
  auto config = config_->Read();
  const uint8_t region = config.heads[selected_head_].region;
  if (!config.ResizeRegion(region, range_knob_.Get())) {
    range_knob_.Set(config.regions[region].range);
    range_rejected_at_ = Now();
    range_flash_on_.reset();
    return;
  }
  config_->Write() = config;
  head_knobs_.position.Set(
      static_cast<float>(config.heads[selected_head_].position) /
      config.regions[region].range);
  range_rejected_at_.reset();
  range_flash_on_.reset();
}

void UI::Tick(uint32_t now) {
  if (!range_rejected_at_.has_value()) {
    return;
  }
  const uint32_t elapsed = now - *range_rejected_at_;
  if (elapsed >= 600) {
    range_rejected_at_.reset();
    range_flash_on_.reset();
    range_knob_.UpdateDisplay();
    return;
  }
  const bool on = (elapsed / 100) % 2 == 0;
  if (range_flash_on_ != on) {
    range_knob_.rgb_led().SetOn(on);
    range_knob_.rgb_led().SetColor(color::RGB(255, 0, 0));
    range_flash_on_ = on;
  }
}

void HeadKnobs::OnChange(Callback<> on_change) {
  EACH_HEAD_KNOB(this, [=](const auto knob) { knob->OnChange(on_change); });
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

void LFOKnobs::OnChange(Callback<> on_change) {
  EACH_LFO_KNOB(this, [=](const auto knob) { knob->OnChange(on_change); });
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
  return max_grain_size.Enabled();
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
    history_.back() = now;

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
