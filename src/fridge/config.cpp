#include "config.hpp"

#include <type_traits>

namespace fridge::config {

TargetObject object_for_parameter(TargetParameter param) {
  switch (param) {
  case TargetParameter::kPosition:
  case TargetParameter::kWriteAmount:
  case TargetParameter::kReadAmount:
  case TargetParameter::kEraseAmount:
  case TargetParameter::kFeedbackAmount:
  case TargetParameter::kPan:
    return TargetObject::kHead;
  case TargetParameter::kRange:
  case TargetParameter::kMaxGrainSize:
  case TargetParameter::kMinGrainSize:
  case TargetParameter::kReverseChance:
  case TargetParameter::kTeleportChance:
  case TargetParameter::kPitchShiftChance:
  case TargetParameter::kLowOctaveChance:
  case TargetParameter::kHighOctaveChance:
    return TargetObject::kLFO;
  case TargetParameter::kDry:
  case TargetParameter::kWet:
    return TargetObject::kMixer;
    break;
  default:
    assert(false);
  }
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
      targets[i] = std::nullopt;
      static_assert(std::is_trivially_copyable<Target>());
      std::copy(&targets[i + 1], &targets[targets.size()], &targets[i]);
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

}  // namespace fridge::config
