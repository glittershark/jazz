#include "regions.hpp"

#include <cassert>
#include <cmath>
#include <limits>

namespace fridge::regions {

size_t WrapPosition(float position, size_t range) {
  if (range == 0 || !std::isfinite(position)) {
    return 0;
  }
  const float limit = static_cast<float>(range);
  float wrapped = position;
  if (wrapped >= limit) {
    wrapped -= limit;
    if (wrapped >= limit) {
      wrapped = std::fmod(wrapped, limit);
    }
  } else if (wrapped < 0.0f) {
    wrapped += limit;
    if (wrapped < 0.0f) {
      wrapped = std::fmod(wrapped, limit);
      if (wrapped < 0.0f) {
        wrapped += limit;
      }
    }
  }
  return static_cast<size_t>(std::lround(wrapped)) % range;
}

Memory::Memory() {
  static_assert(kRegionPageCount <= std::numeric_limits<uint16_t>::max());
  for (size_t i = 0; i < free_pages_.size(); ++i) {
    free_pages_[i] = static_cast<uint16_t>(free_pages_.size() - i - 1);
  }
}

bool Memory::SetRegions(
    const std::array<config::Region, kNumRegions>& regions) {
  if (initialized_ && regions == regions_) {
    return true;
  }
  if (!config::RegionsFit(regions)) {
    return false;
  }

  // Release all shrinking regions first, so an atomic redistribution does
  // not depend on region order.
  for (size_t i = 0; i < regions.size(); ++i) {
    Shrink(i, regions[i].range);
  }
  for (size_t i = 0; i < regions.size(); ++i) {
    Grow(i, regions[i].range);
  }
  regions_ = regions;
  initialized_ = true;
  return true;
}

Memory::Address Memory::Resolve(uint8_t region, size_t position) {
  assert(initialized_ && region < kNumRegions);
  const size_t local = position % regions_[region].range;
  const size_t page = pages_[region][local / kRegionPageSize];
  const size_t offset = local % kRegionPageSize;
  auto& samples = initialized_samples_[page];
  if (fresh_pages_[page]) {
    samples.reset();
    fresh_pages_[page] = false;
  }
  const bool fresh = !samples[offset];
  samples[offset] = true;
  return {.index = page * kRegionPageSize + offset, .fresh = fresh};
}

void Memory::Shrink(size_t region, size_t range) {
  const size_t count = (range + kRegionPageSize - 1) / kRegionPageSize;
  while (page_counts_[region] > count) {
    free_pages_[free_count_++] = pages_[region][--page_counts_[region]];
  }
  if (initialized_ && range < regions_[region].range &&
      range % kRegionPageSize != 0) {
    const size_t page = pages_[region][count - 1];
    // Retain the prefix, discard the truncated tail even if we later grow
    // back into the very same page.
    std::bitset<kRegionPageSize> prefix;
    prefix.set();
    prefix >>= kRegionPageSize - range % kRegionPageSize;
    initialized_samples_[page] &= prefix;
  }
}

void Memory::Grow(size_t region, size_t range) {
  const size_t count = (range + kRegionPageSize - 1) / kRegionPageSize;
  while (page_counts_[region] < count) {
    const size_t page = free_pages_[--free_count_];
    pages_[region][page_counts_[region]++] = static_cast<uint16_t>(page);
    fresh_pages_[page] = true;
  }
}

}  // namespace fridge::regions
