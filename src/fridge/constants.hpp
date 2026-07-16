#ifndef CONSTANTS_H_
#define CONSTANTS_H_

#include <cstddef>

namespace fridge {

constexpr const size_t NUM_HEADS = 8;
constexpr const size_t NUM_LFOS = 8;
constexpr const size_t MAX_TARGET_PARAMS = 64;  // ??

/** Cap on simultaneously-active modulation routes across all LFOs. */
constexpr const size_t MAX_PATCHES = 128;

constexpr const size_t BUFFER_LEN = 44100 * 60 * 6; /* 6 minutes */

/** How long to fade updates to the audio buffer, in samples */
constexpr const size_t FADE_TIME = 128;

// A frame can carry up to 2 contributions per head while a fade is active,
// each posting a write and an erase that live for FADE_TIME samples.
constexpr const size_t UPDATE_CAP =
    ((NUM_HEADS * 2 + 1) * ((FADE_TIME + 1) * 2));

}  // namespace fridge

#endif  // CONSTANTS_H_
