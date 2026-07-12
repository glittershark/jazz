#ifndef STEREO_SAMPLE_H_
#define STEREO_SAMPLE_H_

#include <rapidcheck/gen/Build.h>

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

  /** Fold the sample back down to mono */
  constexpr float Mono() const { return (left * 0.5) + (right * 0.5); }

  /** Pan the sample by an amount */
  constexpr StereoSample Panned(Pan pan) const {
    auto channels = pan.channels();
    return {
        .left = left * channels.left,
        .right = right * channels.right,
    };
  }
};

std::ostream& operator<<(std::ostream& out, const StereoSample& val) {
  return out << "StereoSample{.left =" << val.left << ", .right =" << val.right
             << "}";
}

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
