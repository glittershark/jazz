#ifndef CONSTANTS_H_
#define CONSTANTS_H_

#include <cstddef>

namespace fridge {

constexpr const size_t NUM_HEADS = 4;
constexpr const size_t NUM_LFOS = 4;
constexpr const size_t MAX_TARGET_PARAMS = 8;  // ??

constexpr const size_t BUFFER_LEN = 44100 * 60 * 3; /* 3 minutes */

/** How long to fade updates to the audio buffer, in samples */
constexpr const size_t FADE_TIME = 80;

constexpr const size_t UPDATE_CAP =
    ((NUM_HEADS + 1) * ((FADE_TIME + 1) * 2)) * 4;

}  // namespace fridge

#endif  // CONSTANTS_H_
