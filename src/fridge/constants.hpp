#ifndef CONSTANTS_H_
#define CONSTANTS_H_

#include <cstddef>

namespace fridge {

constexpr const size_t kNumHeads = 10;
constexpr const size_t kNumLfos = 10;
constexpr const size_t kNumRegions = 6;
constexpr const size_t kMaxTargetParams = 8;  // ??

/** Every (LFO, target) pair can be an active modulation route. */
constexpr const size_t kMaxPatches = kNumLfos * kMaxTargetParams;

constexpr const size_t kSampleRateHz = 48000;
constexpr const size_t kBufferLen = 7'938'000;  // Fixed SDRAM budget: ~165 s at 48 kHz.

// Regions share this pool in small pages; their audible lengths remain exact.
constexpr size_t kRegionPageSize = 1024;
constexpr size_t kRegionPageCount = kBufferLen / kRegionPageSize;
constexpr size_t kRegionCapacity = kRegionPageCount * kRegionPageSize;

/** How long to fade updates to the audio buffer, in samples */
constexpr const size_t kFadeTime = 8;

// A frame can carry up to 2 contributions per head while a fade is active,
// each posting a write and an erase that live for FADE_TIME samples.
constexpr const size_t kUpdateCap =
    ((kNumHeads * 2 + 1) * ((kFadeTime + 1) * 2)) * 2;

}  // namespace fridge

#endif  // CONSTANTS_H_
