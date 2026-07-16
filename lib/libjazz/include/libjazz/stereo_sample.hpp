#ifndef STEREO_SAMPLE_H_
#define STEREO_SAMPLE_H_

#include <algorithm>
#include <ostream>

#include "libjazz/pan.hpp"

namespace jazz {
namespace audio {

/** A stereo sample, represented as a pair of (left, right) samples */
struct StereoSample {
  /** The sample for the left channel, in range [0.0f, 1.0f] */
  float left;

  /** The sample for the right channel, in range [0.0f, 1.0f] */
  float right;

  /** Construct a stereo sample from a mono sample */
  constexpr static StereoSample OfMono(float mono_sample) {
    return {.left = mono_sample, .right = mono_sample};
  }

  constexpr static StereoSample Zero() { return OfMono(0.f); }

  /** Fold the sample back down to mono */
  constexpr float Mono() const { return (left * 0.5) + (right * 0.5); }

  /** Pan the sample by an amount.
   *
   * Uses a linear panning law: the attenuated channel fades out linearly while
   * the boosted channel stays at unity. This preserves signal volume at center.
   */
  constexpr StereoSample Panned(Pan pan) const {
    float p = pan.pan();
    return {
        .left = left * std::clamp(1.f - p, 0.f, 1.f),
        .right = right * std::clamp(1.f + p, 0.f, 1.f),
    };
  }

  constexpr bool operator==(const StereoSample& other) const = default;

  constexpr StereoSample operator*(const float& other) const {
    return {
        .left = left * other,
        .right = right * other,
    };
  }

  constexpr StereoSample operator*(const StereoSample& other) const {
    return {
        .left = left * other.left,
        .right = right * other.right,
    };
  }

  constexpr StereoSample operator+(const StereoSample& other) const {
    return {
        .left = left + other.left,
        .right = right + other.right,
    };
  }

  constexpr StereoSample operator-(const StereoSample& other) const {
    return {
        .left = left - other.left,
        .right = right - other.right,
    };
  }

  constexpr StereoSample& operator+=(const StereoSample& other) {
    left += other.left;
    right += other.right;
    return *this;
  }

  constexpr StereoSample Clip(float min, float max) {
    return {
        .left = std::clamp(left, min, max),
        .right = std::clamp(right, min, max),
    };
  }

  constexpr StereoSample Clip() { return Clip(-1.0f, 1.0f); }
};

std::ostream& operator<<(std::ostream& out, const StereoSample& val);

}  // namespace audio
}  // namespace jazz

#ifdef UNIT_TEST

#include <rapidcheck.h>

#include "libjazz/gen.hpp"

namespace rc {
template <>
struct Arbitrary<jazz::audio::StereoSample> {
  static Gen<jazz::audio::StereoSample> arbitrary() {
    return rc::gen::build<jazz::audio::StereoSample>(
        gen::set(&jazz::audio::StereoSample::left, jazz::gen::ratio()),
        gen::set(&jazz::audio::StereoSample::right, jazz::gen::ratio()));
  }
};
}  // namespace rc

#endif  // UNIT_TEST

#endif  // STEREO_SAMPLE_H_
