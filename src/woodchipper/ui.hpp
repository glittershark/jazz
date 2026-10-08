#ifndef UI_H_
#define UI_H_

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

#include "callback.hpp"
#include "libjazz/color.hpp"
#include "rgb_led.hpp"
#include "value_display.hpp"

#ifndef UNIT_TEST
#include "daisy_seed.h"
#endif

namespace fridge::ui {

#ifndef UNIT_TEST
extern daisy::DaisySeed hw;
#endif

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
  bool enabled_ = true;
  Callback<> on_change_;

  static void callback(Knob* this_, int ticks, float turns) {
    this_->Increment(ticks, turns);
  }

 public:
  bool logging_enabled() const { return logging_enabled_; }

  bool Enabled() const { return enabled_; }
  void SetEnabled(bool enabled) { enabled_ = enabled; }

  void OnChange(Callback<> on_change) { on_change_ = on_change; }

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
    if (!Enabled()) {
      return value_;
    }
    auto& res = value_.Increment(ticks, turns);
    LogValue();
    on_change_();
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
    if (Knob<V>::Enabled()) {
      UpdateDisplay();
    }
    return res;
  }

  RgbLedValueDisplay<VD>& value_display() { return value_display_; }
  RgbLed& rgb_led() { return value_display_; }
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

using SingleTurn = Bounded<Turns, 0.0f, 1.0f>;

/** Integer value from 0 to 32 (inclusive), one step per encoder tick */
using ZeroTo32 = Bounded<Ticks<int>, 0, 32>;

template <DisplayableBackingValue V>
using CieInterpKnob = KnobWithDisplay<V, value_display::CieInterp>;

class UI {
  CieInterpKnob<ZeroTo32> knob_;

 public:
  UI(io::led::Controller& led_controller);

  CieInterpKnob<ZeroTo32>& knob() { return knob_; }
};

}  // namespace fridge::ui

#endif  // UI_H_
