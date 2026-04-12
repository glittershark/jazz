#ifndef UI_H_
#define UI_H_

#include <sys/types.h>

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <type_traits>

#include "callback.hpp"
#include "config.hpp"

namespace fridge::ui {

// TODO(nausicaa): arithmetic concepts or something

template <typename V>
concept BackingValue = std::is_arithmetic_v<typename V::Raw_type> &&
                       std::convertible_to<V, typename V::Raw_type> &&
                       requires(const int ticks, const float turns) {
                         {
                           V::Raw(ticks, turns)
                         } -> std::convertible_to<typename V::Raw_type>;
                       } && requires(V v, V v1, V v2, typename V::Raw_type r) {
                         v1 + r;
                         v1 + v2;
                         v1 += r;
                         v1 += v2;
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

  V::Raw_type Value() const { return value_; };
  operator typename V::Raw_type() const { return value_; };

  void Set(const V& value) { value_ = value; };
  Knob& operator=(const V& rhs) {
    Set(rhs);
    return *this;
  }

  void Increment(int ticks, float turns) { value_.Increment(ticks, turns); }
};

template <BackingValue V>
class Infinite {
  V value_;

 public:
  using Raw_type = V::Raw_type;
  static Raw_type Raw(const int ticks, const float turns) {
    return V::Raw(ticks, turns);
  }

  Infinite() = default;
  Infinite(const int ticks, const float turns) : value_(ticks, turns) {}
  Infinite(const V value) : value_(value) {}

  Infinite& Increment(const int ticks, const float turns) {}

  operator Raw_type() const { return value_; }

  Infinite operator+(const Infinite& rhs) const {
    return Infinite(value_ + rhs.value_);
  }

  Infinite& operator+=(const Infinite& rhs) {
    *this = *this + rhs;
    return *this;
  }

  Infinite& operator=(const V& rhs) {
    value_ = rhs;
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
  static Raw_type Raw(const int ticks, const float turns) {
    return V::Raw(ticks, turns);
  }

  Bounded() : value_(saturate(V())) {}
  Bounded(const int ticks, const float turns)
      : value_(saturate(V(ticks, turns))) {}
  Bounded(const Raw_type& raw) : value_(saturate(raw)) {}

  operator Raw_type() const { return value_; }

  Bounded operator+(const V& rhs) const { return Bounded(value_ + rhs); }
  Bounded operator+(const Raw_type& rhs) const { return Bounded(value_ + rhs); }

  Bounded& operator+=(const V& rhs) {
    *this = *this + rhs;
    return *this;
  }

  Bounded& operator+=(const Raw_type& rhs) {
    *this = *this + rhs;
    return *this;
  }

  Bounded& operator=(const V& rhs) {
    value_ = saturate(rhs);
    return *this;
  }
};

template <typename V>
  requires std::is_integral_v<V>
struct Ticks {
  using Raw_type = V;
  Raw_type value;

  static Raw_type Raw(const int ticks, const float) { return Raw_type{ticks}; }

  Ticks() = default;
  Ticks(const int ticks, const float) : value(ticks) {}
  Ticks(const Raw_type& value) : value(value) {}

  operator Raw_type() const { return value; }

  Ticks operator+(const Raw_type& rhs) { return Ticks(value + rhs); }
  Ticks& operator+=(const Raw_type& rhs) {
    value += rhs;
    return *this;
  }
};

struct Turns {
  using Raw_type = float;
  Raw_type value;

  static Raw_type Raw(const int, const float turns) { return Raw_type{turns}; }

  Turns() = default;
  Turns(const int, const float turns) : value(turns) {}
  Turns(const Raw_type& value) : value(value) {}

  operator Raw_type() const { return value; }

  Turns operator+(const Raw_type& rhs) { return Turns(value + rhs); }
  Turns& operator+=(const Raw_type& rhs) {
    value += rhs;
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
