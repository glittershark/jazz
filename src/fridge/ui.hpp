#ifndef UI_H_
#define UI_H_

#include <sys/types.h>

#include <algorithm>
#include <cstddef>

#include "callback.hpp"
#include "config.hpp"

namespace fridge::ui {

class Button {};

template <typename V>
class Knob {
  V value_;

  static void callback(Knob* this_, int ticks, float turns) {
    this_->RawIncrement(ticks, turns);
  }

 public:
  Callback<int, float> GetCallback() {
    return {
        .callback = Knob::callback,
        .data = this,
    };
  }

  V Value() const { return value_; };
  operator V() const { return Value(); };

  void Set(V value) { value_ = value; };
  void RawIncrement(int ticks, float turns) { value_ += V(ticks, turns); }
};

template <typename V>
class Infinite {
  V value_;

 public:
  Infinite() = default;
  Infinite(const int ticks, const float turns) : value_(ticks, turns) {}
  Infinite(const V value) : value_(value) {}

  operator V() const { return value_; }

  Infinite operator+(const Infinite& rhs) const {
    return Infinite(value_ + rhs.value_);
  }

  Infinite& operator=(const V& rhs) {
    value_ = rhs;
    return *this;
  }
};

template <typename V, V::Repr min, V::Repr max>
class Bounded {
  V value_;

  constexpr static V saturate(const V raw) {
    return std::min(max, std::max(min, raw.value));
  };

 public:
  Bounded() : value_(saturate(V())) {}
  Bounded(const int ticks, const float turns)
      : value_(saturate(V(ticks, turns))) {}
  Bounded(const V raw) : value_(saturate(raw)) {}

  operator V() const { return value_; }

  Bounded operator+(const Bounded& rhs) const {
    return Bounded(value_ + rhs.value_);
  }

  Bounded& operator=(const V& rhs) {
    value_ = saturate(rhs);
    return *this;
  }
};

struct Ticks {
  using Repr = ssize_t;
  Repr value;

  Ticks() = default;
  Ticks(const int ticks, const float) : value(ticks) {}
  Ticks(const Repr& value) : value(value) {}

  operator Repr() const { return value; }
};

struct Turns {
  using Repr = float;
  Repr value;

  Turns() = default;
  Turns(const int, const float turns) : value(turns) {}
  Turns(const Repr& value) : value(value) {}

  operator Repr() const { return value; }
};

using SingleTurn = Bounded<Turns, 0.0f, 1.0f>;
using Position = Infinite<Ticks>;
using Size = Bounded<Ticks, 0, 0xff>;  // TODO(nausicaa): is this sane?

struct Head {
  Knob<Position> position;
  Knob<SingleTurn> write_amount;
  Knob<SingleTurn> read_amount;
  Knob<SingleTurn> erase_amount;
  Knob<SingleTurn> feedback;

  void Select(const fridge::config::Head& head);
  fridge::config::Head Config() const;
};

struct LFO {
  Knob<Size> range;
  Knob<Size> max_grain_size;
  Knob<Size> min_grain_size;
  Knob<SingleTurn> reverse_chance;
  Knob<SingleTurn> teleport_chance;
  Knob<SingleTurn> pitch_shift_chance;
  Knob<SingleTurn> low_octave_chance;
  Knob<SingleTurn> high_octave_chance;

  void Select(const fridge::config::LFO& lfo);
  fridge::config::LFO Config() const;
};

struct UI {
  size_t selected_head;
  size_t selected_lfo;

  Head head;
  LFO lfo;

  Knob<SingleTurn> dry;
  Knob<SingleTurn> wet;
  Button tempo;

  // TODO(nausicaa): head selection buttons, etc.

  // TODO(nausicaa): LEDs (there are a bunch, one for each encoder and selection
  // button)
  void UpdateConfig(fridge::config::Config& config) const;
};

fridge::config::Config& operator|=(fridge::config::Config& config, UI& ui);

}  // namespace fridge::ui

#endif  // UI_H_
