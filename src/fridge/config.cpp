#include "config.hpp"

#include <type_traits>

namespace fridge::config {

namespace {
size_t ScalePosition(size_t position, size_t old_range, size_t new_range) {
  if (old_range == 0 || new_range == 0) {
    return 0;
  }
  return static_cast<uint64_t>(position % old_range) * new_range / old_range;
}
}  // namespace

bool Config::AssignRegion(size_t head, size_t region) {
  if (!CanAssignRegion(head, region)) {
    return false;
  }
  Head& selected = heads[head];
  selected.position = ScalePosition(
      selected.position, regions[selected.region].range, regions[region].range);
  selected.region = static_cast<uint8_t>(region);
  return true;
}

bool Config::ResizeRegion(size_t region, size_t range) {
  if (!CanResizeRegion(region, range)) {
    return false;
  }
  for (Head& head : heads) {
    if (head.region == region) {
      head.position =
          ScalePosition(head.position, regions[region].range, range);
    }
  }
  regions[region].range = range;
  return true;
}

ToggleResult LFO::ToggleTarget(const Target& target) {
  // 1. attempt to disable a preexisting target
  size_t i;
  for (i = 0; i < targets.size(); ++i) {
    const auto& existing_target = targets[i];
    if (!existing_target.has_value()) {
      break;
    }

    if (*existing_target == target) {
      static_assert(std::is_trivially_copyable<Target>());
      std::copy(targets.begin() + i + 1, targets.end(), targets.begin() + i);
      targets.back() = std::nullopt;
      return ToggleResult::kToggledOff;
    }
  }

  // 2. Otherwise, add the new target at the end (unless we're full)
  if (i >= targets.size()) {
    return ToggleResult::kNothingHappened;
  }
  targets[i] = target;
  return ToggleResult::kToggledOn;
}

// ----- Validation

bool Config::CanAssignRegion(size_t head, size_t region) const {
  return head < heads.size() && region < regions.size() &&
         heads[head].region < regions.size() && RegionsFit(regions);
}

bool Config::CanResizeRegion(size_t region, size_t range) const {
  if (region >= regions.size()) {
    return false;
  }
  auto proposed = regions;
  proposed[region].range = range;
  return RegionsFit(proposed);
}

bool RegionsFit(const std::array<Region, kNumRegions>& regions) {
  size_t available = kRegionPageCount;
  for (const Region& region : regions) {
    if (region.range == 0 || region.range > kRegionCapacity) {
      return false;
    }
    const size_t pages = (region.range + kRegionPageSize - 1) / kRegionPageSize;
    if (pages > available) {
      return false;
    }
    available -= pages;
  }
  return true;
}

}  // namespace fridge::config
