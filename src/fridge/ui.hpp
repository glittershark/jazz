#ifndef UI_H_
#define UI_H_

#include <sys/types.h>

#include <algorithm>
#include <cassert>
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

  void RawIncrement(int ticks, float turns) { value_ += V(ticks, turns); }
};

template <typename V>
class Infinite {
  V value_;

 public:
  using Raw_type = V::Raw_type;

  Infinite() = default;
  Infinite(const int ticks, const float turns) : value_(ticks, turns) {}
  Infinite(const V value) : value_(value) {}

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

template <typename V, V::Raw_type min, V::Raw_type max>
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
  Bounded(const int ticks, const float turns)
      : value_(saturate(V(ticks, turns))) {}
  Bounded(const Raw_type raw) : value_(saturate(raw)) {}

  operator Raw_type() const { return value_; }

  Bounded operator+(const V& rhs) const { return Bounded(value_ + rhs); }

  Bounded& operator+=(const V& rhs) {
    *this = *this + rhs;
    return *this;
  }

  Bounded& operator=(const V& rhs) {
    value_ = saturate(rhs);
    return *this;
  }
};

template <typename V>
struct Ticks {
  using Raw_type = V;
  Raw_type value;

  Ticks() = default;
  Ticks(const int ticks, const float) : value(ticks) {}
  Ticks(const Raw_type& value) : value(value) {}

  operator Raw_type() const { return value; }
};

struct Turns {
  using Raw_type = float;
  Raw_type value;

  Turns() = default;
  Turns(const int, const float turns) : value(turns) {}
  Turns(const Raw_type& value) : value(value) {}

  operator Raw_type() const { return value; }
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
