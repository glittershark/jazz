#ifndef UI_H_
#define UI_H_

#include <sys/types.h>

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <limits>
#include <numeric>
#include <type_traits>

#include "callback.hpp"
#include "config.hpp"

namespace fridge::ui {

template <typename V>
concept BackingValue =
    std::convertible_to<V, typename V::Raw_type> &&
    std::convertible_to<typename V::Raw_type, V> &&
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

class Button {};

template <BackingValue V>
class Knob {
  V value_;

  static void callback(Knob* this_, int ticks, float turns) {
    this_->Increment(ticks, turns);
  }

 public:
  Knob() = default;
  Knob(Knob&&) = delete;
  Knob(const Knob&) = delete;

  TypedCallback<Knob, int, float> GetCallback() {
    return {
        .callback = Knob::callback,
        .data = this,
    };
  }

  operator typename V::Raw_type() const { return value_; };

  Knob& operator=(const V& rhs) {
    value_ = rhs;
    return *this;
  }

  V& Increment(int ticks, float turns) {
    return value_.Increment(ticks, turns);
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

  operator Raw_type() const { return value_; }

  Bounded operator+(const Bounded& rhs) const {
    return Bounded(value_ + rhs.value_);
  }

  Bounded& operator=(const Bounded& rhs) = default;

  Bounded& operator+=(const Bounded& rhs) {
    *this = *this + rhs;
    return *this;
  }
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
      value_ = value_ < min - ticks ? min : value_ - ticks;
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
};

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
};

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
};

using SingleTurn = Bounded<Turns, 0.0f, 1.0f>;
using Position = Bounded<Ticks<size_t>, 0, BUFFER_LEN>;
using Size = Bounded<Ticks<size_t>, 0, 0xff>;  // TODO(nausicaa): is this sane?

struct Head {
  Knob<Position> position;
  Knob<SingleTurn> write_amount;
  Knob<SingleTurn> read_amount;
  Knob<SingleTurn> erase_amount;
  Knob<Feedback> feedback;

  void Select(const config::Head& head);
  config::Head Config() const;
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

  void Select(const config::LFO& lfo);
  config::LFO Config() const;
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

  // TODO(nausicaa): LEDs (there are a bunch, one for each encoder and
  // selection button)
  void UpdateConfig(config::Config& config) const;
};

config::Config& operator|=(config::Config& config, UI& ui);

}  // namespace fridge::ui

#endif  // UI_H_
