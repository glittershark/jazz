#ifndef DEFAULT_CONFIG_H_
#define DEFAULT_CONFIG_H_

#include "config.hpp"

namespace fridge::config {

/** The root config both the firmware and the console demo boot with, so a
 * host-side run reproduces the hardware setup. */
inline Config DefaultConfig() {
  Config config;

  config.dry = 0.7f;
  config.wet = 0.8f;

  // Heads default to writing and reading at full amount; make them inert so
  // only the heads configured below touch the tape.
  for (Head& head : config.heads) {
    head.write_amount = 0.0f;
    head.read_amount = 0.0f;
    head.erase_amount = 1.0f;
    head.feedback.amount = 0.0f;
  }

  config.heads[0] = {
      .position = 0,
      .write_amount = 1.0f,
      .read_amount = 0.75f,
      .erase_amount = 0.999f,
  };

  config.lfos[0] = {
      .range = 24000,
      .max_grain_size = 24000,
      .min_grain_size = 24000,
      .reverse_chance = 1.0f,
  };
  config.lfos[0].targets[0] = Target{
      .object = TargetObject::kHead,
      .parameter = TargetParameter::kPosition,
      .object_idx = 0,
  };

  return config;
}

}  // namespace fridge::config

#endif  // DEFAULT_CONFIG_H_
