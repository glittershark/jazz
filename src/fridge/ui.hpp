#ifndef UI_H_
#define UI_H_

#include <sys/types.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <type_traits>

#include "callback.hpp"
#include "config.hpp"
#include "constants.hpp"
#include "io.hpp"
#include "libjazz/color.hpp"
#include "rgb_led.hpp"
#include "value_display.hpp"

namespace fridge::ui {

#ifndef UNIT_TEST
#include "daisy_seed.h"
#include "per/tim.h"
extern daisy::DaisySeed hw;
#endif

constexpr const size_t kHoldDelayMs = 500;
constexpr const size_t kBlinkFreqMs = 500;
// TODO
constexpr const color::RGB kSelectedBlinkColor = color::RGB(0, 150, 0);
constexpr const color::RGB kUnselectedBlinkColor = color::RGB(150, 0, 0);

uint32_t Now();

template <typename V>
concept BackingValue =
    std::convertible_to<V, typename V::Raw_type> &&
    std::convertible_to<typename V::Raw_type, V> &&
    requires(const V& v, const char* key) { v.Print(key); } &&
    requires(const V& vr, const V::Raw_type& raw, int ticks, float turns) {
      V();
      V(vr);
      V(raw);
      V(ticks, turns);
    } && requires(V v, int ticks, float turns) {
      { v.Increment(ticks, turns) } -> std::same_as<V&>;
    } && requires(V v1, const V& v2) {
      { v1 + v2 } -> std::same_as<V>;
      { v1 = v2 } -> std::same_as<V&>;
      { v1 += v2 } -> std::same_as<V&>;
    };

template <typename V>
concept DisplayableBackingValue = BackingValue<V> && requires(const V& v) {
  { v.GetDisplay() } -> std::convertible_to<uint8_t>;
};

template <BackingValue V>
class Knob {
  static void callback(Knob* this_, int ticks, float turns) {
    this_->Increment(ticks, turns);
  }

 public:
  bool logging_enabled() const { return logging_enabled_; }

 protected:
  V value_;
  const char* name_;
  bool logging_enabled_ = false;

  void LogValue() const {
#ifndef UNIT_TEST
    if (logging_enabled()) {
      value_.Print(name_);
    }
#endif
  }

 public:
  Knob(const char* name) : value_(), name_(name) {}
  Knob(Knob&&) = delete;
  Knob(const Knob&) = delete;

  TypedCallback<Knob, int, float> GetCallback() {
    return {
        .callback = Knob::callback,
        .data = this,
    };
  }

  V Get() const { return value_; };
  void Set(const V& rhs) {
    value_ = rhs;
    LogValue();
  }

  V& Increment(int ticks, float turns) {
    auto& res = value_.Increment(ticks, turns);
    LogValue();
    return res;
  }

  void EnableLogging(bool enable = true) { logging_enabled_ = enable; }
};

template <DisplayableBackingValue V, ValueDisplay VD>
class KnobWithDisplay : public Knob<V> {
  static void callback(KnobWithDisplay<V, VD>* this_, int ticks, float turns) {
    this_->Increment(ticks, turns);
  }

  RgbLedValueDisplay<VD> value_display_;

 public:
  void UpdateDisplay() {
    // TODO(aspen): We may want to turn the LEDs on higher up an abstraction
    // level at some point. For now, this should suffice.
    value_display_.SetOn(true);
    value_display_.SetValue(this->Get().GetDisplay());
  }

  KnobWithDisplay(const char* name, RgbLedValueDisplay<VD> value_display)
      : Knob<V>(name), value_display_(value_display) {};

  TypedCallback<KnobWithDisplay<V, VD>, int, float> GetCallback() {
    return {
        .callback = KnobWithDisplay<V, VD>::callback,
        .data = this,
    };
  }

  void Set(const V& rhs) {
    Knob<V>::Set(rhs);
    UpdateDisplay();
  }

  V& Increment(int ticks, float turns) {
    auto& res = Knob<V>::Increment(ticks, turns);
    UpdateDisplay();
    return res;
  }

  RgbLedValueDisplay<VD>& value_display() { return value_display_; }
  RgbLed& rgb_led() { return value_display_; }

  void BlinkOn() {
    rgb_led().SetOn(true);
    rgb_led().SetColor(kUnselectedBlinkColor);
  }
  void BlinkOff() { rgb_led().SetOn(false); }
  void Blink(bool on) {
    if (on) {
      BlinkOn();
    } else {
      BlinkOff();
    }
  }
};

template <BackingValue V>
class Infinite {
  V value_;

 public:
  using Raw_type = V::Raw_type;

  Infinite() = default;
  Infinite(const int ticks, const float turns) : value_(ticks, turns) {}

  Infinite& Increment(const int ticks, const float turns) {}

  operator Raw_type() const { return value_; }

  Infinite operator+(const Infinite& rhs) const {
    return Infinite(value_ + rhs.value_);
  }

  Infinite& operator=(const Infinite& rhs) = default;

  Infinite& operator+=(const Infinite& rhs) {
    *this = *this + rhs;
    return *this;
  }
};

template <BackingValue V, V::Raw_type min, V::Raw_type max>
class Bounded {
  V value_;

  constexpr static V saturate(const V value) {
    return std::min(max, std::max(min,
                                  // i love c++
                                  value.operator typename V::Raw_type()));
  };

 public:
  using Raw_type = V::Raw_type;

  Bounded() : value_(saturate(V())) {}
  Bounded(const Raw_type& raw) : value_(saturate(raw)) {}
  Bounded(const int ticks, const float turns)
      : value_(saturate(V(ticks, turns))) {}

  Bounded& Increment(const int ticks, const float turns) {
    value_ = saturate(value_.Increment(ticks, turns));
    return *this;
  }

  constexpr operator Raw_type() const { return value_; }

  Bounded operator+(const Bounded& rhs) const {
    return Bounded(value_ + rhs.value_);
  }

  Bounded& operator=(const Bounded& rhs) = default;

  Bounded& operator+=(const Bounded& rhs) {
    *this = *this + rhs;
    return *this;
  }

  uint8_t GetDisplay() const {
    return static_cast<uint8_t>(
        (static_cast<float>(value_ - min) / static_cast<float>(max - min)) *
        255.f);
  }

  void Print(const char* key) const { value_.Print(key); }
};

template <typename V>
  requires std::is_integral_v<V>
class Ticks {
  V value_;

 public:
  using Raw_type = V;

  static Raw_type Raw(const int ticks, const float) { return Raw_type{ticks}; }

  Ticks() = default;
  Ticks(const int ticks, const float) : value_(ticks) {}
  Ticks(const Raw_type& value_) : value_(value_) {}

  Ticks& Increment(const int ticks, const float) {
    // value_ = std::add_sat(value_, ticks); // but we can't yet
    if (ticks > 0) {
      const auto max = std::numeric_limits<V>::max();
      value_ = value_ > max - ticks ? max : value_ + ticks;
    } else if (ticks < 0) {
      const auto min = std::numeric_limits<V>::min();
      value_ = value_ < min - ticks ? min : value_ + ticks;
    }

    // if ticks == 0, whatever
    return *this;
  }

  operator Raw_type() const { return value_; }

  Ticks operator+(const Raw_type& rhs) { return Ticks(value_ + rhs); }

  Ticks& operator=(const Ticks& rhs) = default;

  Ticks& operator+=(const Raw_type& rhs) {
    value_ += rhs;
    return *this;
  }

  void Print(const char* key) const {
#ifndef UNIT_TEST
    hw.PrintLine("%s = %d", key, value_);
#endif
  }
};

static_assert(BackingValue<Ticks<size_t>>);

class Turns {
  float value_;

 public:
  using Raw_type = float;

  Turns() = default;
  Turns(const int, const float turns) : value_(turns) {}
  Turns(const Raw_type& raw) : value_(raw) {}

  Turns& Increment(const int, const float turns) {
    value_ += turns;
    return *this;
  }

  operator Raw_type() const { return value_; }

  Turns operator+(const Turns& rhs) const { return Turns(value_ + rhs.value_); }

  Turns& operator=(const Turns& rhs) = default;

  Turns& operator+=(const Raw_type& rhs) {
    value_ += rhs;
    return *this;
  }

  void Print(const char* key) const {
#ifndef UNIT_TEST
    hw.PrintLine("%s = " FLT_FMT(3), key, FLT_VAR3(value_));
#endif
  }
};
static_assert(BackingValue<Turns>);

/**
 * Scales each turn by some integral quantity, backed by an integral value
 *
 * One turn is always equal to SCALE
 * */
template <typename V, V SCALE>
  requires std::is_integral_v<V>
class ScaledIntegralTurns {
  V value_;

  static V ScaleTurns(float turns) { return static_cast<V>(turns * SCALE); };

 public:
  using Raw_type = V;
  ScaledIntegralTurns() = default;
  ScaledIntegralTurns(const int, const float turns) : value_(turns) {}
  ScaledIntegralTurns(const Raw_type& raw) : value_(raw) {}

  ScaledIntegralTurns<V, SCALE>& Increment(const int, const float turns) {
    if (turns > 0) {
      value_ += static_cast<V>(turns * SCALE);
    } else if (turns < 0) {
      auto scaled = static_cast<V>(-turns * SCALE);
      if (scaled >= value_) {
        value_ = 0;
      } else {
        value_ -= scaled;
      }
    }
    return *this;
  }

  operator Raw_type() const { return value_; }

  ScaledIntegralTurns<V, SCALE> operator+(
      const ScaledIntegralTurns<V, SCALE>& rhs) const {
    return ScaledIntegralTurns<V, SCALE>(value_ + rhs.value_);
  }

  ScaledIntegralTurns<V, SCALE>& operator=(
      const ScaledIntegralTurns<V, SCALE>& rhs) = default;

  ScaledIntegralTurns<V, SCALE>& operator+=(const Raw_type& rhs) {
    value_ += rhs;
    return *this;
  }

  void Print(const char* key) const {
#ifndef UNIT_TEST
    hw.PrintLine("%s = %d", key, value_);
#endif
  }
};
static_assert(BackingValue<ScaledIntegralTurns<size_t, 44100>>);

class Feedback {
  using V = Bounded<Turns, -1.0f, 1.0f>;
  V value_;

 public:
  using Raw_type = config::Feedback;

  Feedback() = default;
  Feedback(const int, const float turns) : value_(turns) {}
  Feedback(const V& value) : value_(value) {}
  Feedback(const Raw_type& value)
      : value_(value.kind == config::Feedback::Kind::kRead ? value.amount
                                                           : -value.amount) {}

  Feedback& Increment(const int, const float turns) {
    value_ += turns;
    return *this;
  }

  operator Raw_type() const {
    if (value_ >= 0.0f) {
      return {
          .kind = config::Feedback::Kind::kRead,
          .amount = value_,
      };
    } else {
      return {
          .kind = config::Feedback::Kind::kErase,
          .amount = -value_,
      };
    }
  }

  Feedback operator+(const Feedback& rhs) const {
    return Feedback(value_ + rhs.value_);
  }

  Feedback& operator+=(const Feedback& rhs) {
    *this = *this + rhs;
    return *this;
  }

  Feedback& operator=(const V& rhs) {
    value_ = rhs;
    return *this;
  }

  void Print(const char* key) const {
#ifndef UNIT_TEST
    hw.PrintLine("%s = " FLT_FMT(3), key, FLT_VAR3(value_));
#endif
  }
};

static_assert(BackingValue<Feedback>);

class FeedbackKnob : public Knob<Feedback> {
  RgbLed rgb_led_;
  value_display::CieInterp read_display_;
  value_display::CieInterp erase_display_;

  static void callback(FeedbackKnob* this_, int ticks, float turns) {
    this_->Increment(ticks, turns);
  }

  color::RGB Color() {
    config::Feedback val = Get();
    // Adjust for perceptual brightness curve of our particular LEDs. This
    // function arived at somewhat empirically
    auto adjusted = std::pow(val.amount, 2);
    auto amount = static_cast<uint8_t>(adjusted * 255);
    switch (val.kind) {
    case config::Feedback::Kind::kRead:
      return read_display_(amount);
    case config::Feedback::Kind::kErase:
      return erase_display_(amount);
    default:
      assert(false);
    }
  }

 public:
  struct Config {
    color::RGB max_read_color;
    color::RGB max_erase_color;
    color::RGB zero_color = {0, 0, 0};
  };

  FeedbackKnob(RgbLed rgb_led, Config config)
      : Knob("Feedback"),
        rgb_led_(rgb_led),
        read_display_{.start = config.zero_color, .end = config.max_read_color},
        erase_display_{.start = config.zero_color,
                       .end = config.max_erase_color} {}

  void UpdateDisplay() {
    rgb_led_.SetOn(true);
    rgb_led_.SetColor(Color());
  }

  TypedCallback<FeedbackKnob, int, float> GetCallback() {
    return {
        .callback = FeedbackKnob::callback,
        .data = this,
    };
  }

  void Set(const Feedback& rhs) {
    Knob<Feedback>::Set(rhs);
    UpdateDisplay();
  }

  Feedback& Increment(int ticks, float turns) {
    auto& res = Knob<Feedback>::Increment(ticks, turns);
    UpdateDisplay();
    return res;
  }

  void BlinkOn() {
    rgb_led_.SetOn(true);
    rgb_led_.SetColor(kUnselectedBlinkColor);
  }
  void BlinkOff() { rgb_led_.SetOn(false); }
  void Blink(bool on) {
    if (on) {
      BlinkOn();
    } else {
      BlinkOff();
    }
  }
};

using SingleTurn = Bounded<Turns, 0.0f, 1.0f>;

using OneTurnIsBufferLen =
    Bounded<ScaledIntegralTurns<size_t, kBufferLen>, 0, kBufferLen>;

template <std::uint8_t N>
class RadioButtons {
  std::uint8_t selected_;

 public:
  struct Held {
    std::uint8_t which;
    std::uint32_t since;
  };

 private:
  std::optional<Held> held_;
  std::array<RgbLed, N> leds_;
  color::RGB selected_color_;
  Callback<uint8_t> on_change_;

  struct Selector {
    RadioButtons* rb;
    std::uint8_t which;

    Selector() : rb(nullptr), which(0) {}

    // because doing this at construction is too hard
    void Configure(RadioButtons* rb, std::uint8_t which) {
      this->rb = rb;
      this->which = which;
    }

    void Select(bool state) {
      if (rb) {
        /* NB: the buttons are pull-up and short to *ground* when pressed, so
           "true" is the steady state and "false" means the button is
           currently held */
        auto pressed = !state;

        if (pressed) {
          if (rb->selected_ != which) {
            rb->Select(which);
          }
          if (!rb->held_.has_value()) {
            rb->held_ = Held{.which = which, .since = Now()};
          }
        } else if (rb->selected_ == which) {
          // If we just released the button for the *currently selected*
          // selector (which must have been the last pressed one), then reset
          // the held item to nothing
          rb->held_ = std::nullopt;
        }
      }
    }

    Callback<bool> GetCallback() {
      return {
          .callback = +[](void* self,
                          bool v) { static_cast<Selector*>(self)->Select(v); },
          .data = this,
      };
    }
  };

  std::array<Selector, N> selectors_;

 public:
  RadioButtons(std::array<RgbLed, N> leds, color::RGB selected_color)
      : selected_(0), leds_(leds), selected_color_(selected_color) {
    for (std::uint8_t i = 0; i < N; ++i) {
      selectors_[i].Configure(this, i);
    }
  }

#ifndef UNIT_TEST
  void RegisterCallbacks(std::array<io::Button, N>& buttons) {
    // i miss rust and zip()
    for (std::uint8_t i = 0; i < N; ++i) {
      buttons[i].OnChange(selectors_[i].GetCallback());
    }
  }
#endif  // UNIT_TEST

  void OnChange(Callback<uint8_t> on_change) { on_change_ = on_change; }

  void Select(std::uint8_t which) {
    const bool changed = which != selected_;

    leds_[selected_].SetOn(false);
    leds_[which].SetOn(true);
    leds_[which].SetColor(selected_color_);

    selected_ = which;

    // Only fire on_change when the selection actually moved — the callback
    // does a destructive save-then-load of knob state, so firing it on
    // no-op selects (including the boot-time Select(0)) would clobber the
    // just-loaded initial config.
    if (changed && on_change_) {
      on_change_(which);
    }
  }

  const std::optional<Held>& held() const { return held_; }
  const uint8_t selected() const { return selected_; }

  void StartBlinking() { leds_[selected_].SetOn(false); }
  void StopBlinking() {
    for (size_t i = 0; i < N; ++i) {
      leds_[i].SetOn(false);
    }
    leds_[selected_].SetOn(true);
    leds_[selected_].SetColor(selected_color_);
  }

  void BlinkOn(uint8_t which) {
    leds_[which].SetOn(true);
    auto color =
        which == selected_ ? kSelectedBlinkColor : kUnselectedBlinkColor;
    leds_[which].SetColor(color);
  }

  void BlinkOff(uint8_t which) { leds_[which].SetOn(false); }
};

template <DisplayableBackingValue V>
using CieInterpKnob = KnobWithDisplay<V, value_display::CieInterp>;

struct HeadKnobs {
  CieInterpKnob<OneTurnIsBufferLen> position;
  CieInterpKnob<SingleTurn> write_amount;
  CieInterpKnob<SingleTurn> read_amount;
  CieInterpKnob<SingleTurn> erase_amount;
  FeedbackKnob feedback;
  // NOTE: If you add new knobs here, remember to also add them under
  // EACH_HEAD_KNOB!

  void Select(const config::Head& head);
  config::Head Config() const;

  void StartBlinking();
  void StopBlinking();
};

#define EACH_HEAD_KNOB(head_knobs, f) \
  {                                   \
    f(&head_knobs->position);         \
    f(&head_knobs->write_amount);     \
    f(&head_knobs->read_amount);      \
    f(&head_knobs->erase_amount);     \
    f(&head_knobs->feedback);         \
  }

struct LFOKnobs {
  CieInterpKnob<OneTurnIsBufferLen> range;
  CieInterpKnob<OneTurnIsBufferLen> max_grain_size;
  CieInterpKnob<OneTurnIsBufferLen> min_grain_size;
  CieInterpKnob<SingleTurn> reverse_chance;
  CieInterpKnob<SingleTurn> teleport_chance;
  CieInterpKnob<SingleTurn> pitch_shift_chance;
  CieInterpKnob<SingleTurn> low_octave_chance;
  CieInterpKnob<SingleTurn> high_octave_chance;
  // NOTE: If you add new knobs here, remember to also add them under
  // EACH_LFO_KNOB!

  void Select(const config::LFO& lfo);
  config::LFO Config() const;

  void StartBlinking();
  void StopBlinking();
};

#define EACH_LFO_KNOB(lfo_knobs, f)    \
  {                                    \
    f(&lfo_knobs->range);              \
    f(&lfo_knobs->max_grain_size);     \
    f(&lfo_knobs->min_grain_size);     \
    f(&lfo_knobs->reverse_chance);     \
    f(&lfo_knobs->teleport_chance);    \
    f(&lfo_knobs->pitch_shift_chance); \
    f(&lfo_knobs->low_octave_chance);  \
    f(&lfo_knobs->high_octave_chance); \
  }

#ifndef UNIT_TEST
class TempoButton {
  constexpr const static size_t HISTORY_LENGTH = 2;

  // times are in milliseconds
  // TODO(nausicaa): use some ringbuffer class here instead
  std::array<uint32_t, HISTORY_LENGTH> history_;
  uint32_t average_gap_;

  void Tick(bool state);

 public:
  TempoButton();

  Callback<bool> GetCallback() {
    return {
        .callback = +[](void* self,
                        bool v) { static_cast<TempoButton*>(self)->Tick(v); },
        .data = this,
    };
  }

  float Estimate();
};
#else
struct TempoButton {
  void Tick(bool state) {}
  TempoButton() = default;

  Callback<bool> GetCallback() {
    return {
        .callback = +[](void* self,
                        bool v) { static_cast<TempoButton*>(self)->Tick(v); },
        .data = this,
    };
  }

  float Estimate() { return 0; }
};
#endif  // UNIT_TEST

struct UI {
 private:
  // The authoritative, live config of the effect.
  config::Config config_;

  struct BlinkState {
    bool blink;
    uint32_t last_toggle;
  };

  // If anything is blinking, whether it is currently on or off.
  std::optional<BlinkState> blink_state_ = std::nullopt;
  bool AmBlinking() const { return blink_state_.has_value(); }

#ifndef UNIT_TEST
  daisy::TimerHandle timer_;
#endif  // UNIT_TEST

  config::Target TargetForSelected(config::TargetParameter param) const;

 public:
  void Tick();

 private:
  static void timer_callback_(void* self) { static_cast<UI*>(self)->Tick(); }

 public:
  uint8_t selected_head = 0;
  uint8_t selected_lfo = 0;

  HeadKnobs head_knobs;
  LFOKnobs lfo_knobs;

  CieInterpKnob<SingleTurn> dry_knob;
  CieInterpKnob<SingleTurn> wet_knob;
  TempoButton tempo_button;

  RadioButtons<kNumHeads> head_select;
  RadioButtons<kNumLfos> lfo_select;

  // TODO(nausicaa): LEDs (there are a bunch, one for each encoder and
  // selection button)

  const config::Config& Config();

  UI(io::led::Controller& led_controller,
     config::Config initial_config = config::Config{});

  void KnobPressed(config::TargetParameter param, bool pressed);
};

}  // namespace fridge::ui

#endif  // UI_H_
