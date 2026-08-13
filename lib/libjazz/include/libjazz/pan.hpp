#ifndef PAN_H_
#define PAN_H_

#include <cassert>
#include <iostream>

namespace jazz {
namespace audio {

/** A Pan amount, expressed as a float in the range [-1.0f, 1.0f]. -1.0f is full
 * left, 1.0f is full right.
 */
class Pan {
  float pan_;

 public:
  constexpr explicit Pan(float pan) : pan_(pan) {
    assert(pan >= -1.0f && pan <= 1.0f);
  }
  static constexpr const Pan Center() { return Pan(0.f); }

  constexpr float pan() const { return pan_; }
  constexpr float& pan() { return pan_; }

  constexpr bool operator==(const Pan&) const = default;

  /** A Pan amount expressed as the volumes of the left and the right channels.
   */
  struct Channels {
    /** Volume of the left channel, in the range [0.f, 1.0f]. */
    float left;

    /** Volume of the right channel, in the range [0.f, 1.0f]. */
    float right;

    constexpr bool operator==(const Channels&) const = default;

    /** Convert back to a Pan */
    constexpr Pan pan() { return Pan(1 - 2 * left); }

    constexpr bool valid() {
      return (left + right == 1.0f) && (left >= 0.f && left <= 1.0f) &&
             (right >= 0.f && right <= 1.0f);
    }
  };

  constexpr Channels channels() const {
    return {
        .left = 0.5f - (0.5f * pan_),
        .right = 0.5f + (0.5f * pan_),
    };
  };

  /** Pan to the left by amount */
  static constexpr const Pan Left(float amount) {
    assert(amount >= 0.0f && amount <= 1.0f);
    return Pan(-amount);
  }

  /** Pan to the right by amount */
  static constexpr const Pan Right(float amount) {
    assert(amount >= 0.0f && amount <= 1.0f);
    return Pan(amount);
  }

  constexpr bool IsLeft() const { return pan_ < 0.f; }
  constexpr bool IsRight() const { return pan_ > 0.f; }
  constexpr bool IsCenter() const { return pan_ == 0.f; }
};

std::ostream& operator<<(std::ostream& out, const Pan& val);
std::ostream& operator<<(std::ostream& out, const Pan::Channels& val);

}  // namespace audio
}  // namespace jazz

#ifdef UNIT_TEST

#include <rapidcheck.h>

#include "libjazz/gen.hpp"

namespace rc {
template <>
struct Arbitrary<jazz::audio::Pan> {
  static Gen<jazz::audio::Pan> arbitrary() {
    return rc::gen::construct<jazz::audio::Pan>(jazz::gen::signedRatio());
  }
};

}  // namespace rc

#endif  // UNIT_TEST

#endif  // PAN_H_
