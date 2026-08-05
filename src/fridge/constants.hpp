#ifndef CONSTANTS_H_
#define CONSTANTS_H_

#include <cstddef>

namespace fridge {

constexpr const size_t kNumHeads = 8;
constexpr const size_t kNumLfos = 8;
constexpr const size_t kMaxTargetParams = 8;  // ??

/** Every (LFO, target) pair can be an active modulation route. */
constexpr const size_t kMaxPatches = kNumLfos * kMaxTargetParams;

constexpr const size_t kSampleRateHz = 44100;
constexpr const size_t kBufferLen = kSampleRateHz * 60 * 3; /* 3 minutes */

/** How long to fade updates to the audio buffer, in samples */
constexpr const size_t kFadeTime = 8;

// A frame can carry up to 2 contributions per head while a fade is active,
// each posting a write and an erase that live for FADE_TIME samples.
constexpr const size_t kUpdateCap =
    ((kNumHeads * 2 + 1) * ((kFadeTime + 1) * 2)) * 2;

}  // namespace fridge

#endif  // CONSTANTS_H_
