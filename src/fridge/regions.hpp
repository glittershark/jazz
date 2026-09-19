#ifndef FRIDGE_REGIONS_H_
#define FRIDGE_REGIONS_H_

#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>

#include "config.hpp"

namespace fridge::regions {

size_t WrapPosition(float position, size_t range);

/** Independent circular buffers backed by a shared pool. Page mappings keep
 * existing samples stationary when another region grows or shrinks. No heap
 * allocation, audio copies, or bulk sample clearing occurs on resize. */
class Memory {
 public:
  struct Address {
    size_t index;
    bool fresh;
  };

  Memory();
  bool SetRegions(const std::array<config::Region, kNumRegions>& regions);
  Address Resolve(uint8_t region, size_t position);

 private:
  std::array<config::Region, kNumRegions> regions_{};
  std::array<size_t, kNumRegions> page_counts_{};
  std::array<std::array<uint16_t, kRegionPageCount>, kNumRegions> pages_{};
  std::array<uint16_t, kRegionPageCount> free_pages_{};
  size_t free_count_ = kRegionPageCount;
  bool initialized_ = false;
  std::bitset<kRegionPageCount> fresh_pages_;
  std::array<std::bitset<kRegionPageSize>, kRegionPageCount>
      initialized_samples_;

  void Shrink(size_t region, size_t range);
  void Grow(size_t region, size_t range);
};

}  // namespace fridge::regions

#endif
