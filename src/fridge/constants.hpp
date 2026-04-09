#ifndef CONSTANTS_H_
#define CONSTANTS_H_

#include <cstddef>

constexpr const size_t NUM_HEADS = 8;
constexpr const size_t NUM_LFOS = 8;
constexpr const size_t MAX_TARGET_PARAMS = 64;  // ??
constexpr const size_t MAX_FADE_TIME = 128;
constexpr const size_t UPDATE_CAP =
    ((NUM_HEADS + 1) * ((MAX_FADE_TIME + 1) * 2));

#endif  // CONSTANTS_H_
