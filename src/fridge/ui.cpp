#include "ui.hpp"

#include "config.hpp"

namespace fridge::ui {

config::Head Head::Config() const {
  return {
      .position = position,
      .write_amount = write_amount,
      .read_amount = read_amount,
      .erase_amount = erase_amount,
      .feedback = feedback,
  };
};

void Head::Select(const fridge::config::Head& head) {
  position = head.position;
  write_amount = head.write_amount;
  read_amount = head.read_amount;
  erase_amount = head.erase_amount;
  feedback = head.feedback;
}

fridge::config::LFO LFO::Config() const {
  return {
      .range = range,
      .max_grain_size = max_grain_size,
      .min_grain_size = min_grain_size,
      .reverse_chance = reverse_chance,
      .teleport_chance = teleport_chance,
      .pitch_shift_chance = pitch_shift_chance,
      .low_octave_chance = low_octave_chance,
      .high_octave_chance = high_octave_chance,
  };
}

void LFO::Select(const fridge::config::LFO& lfo) {
  range = lfo.range;
  max_grain_size = lfo.max_grain_size;
  min_grain_size = lfo.min_grain_size;
  reverse_chance = lfo.reverse_chance;
  teleport_chance = lfo.teleport_chance;
  pitch_shift_chance = lfo.pitch_shift_chance;
  low_octave_chance = lfo.low_octave_chance;
  high_octave_chance = lfo.high_octave_chance;
}

void fridge::ui::UI::UpdateConfig(fridge::config::Config& config) const {
  config.heads[selected_head] = head.Config();
  config.lfos[selected_lfo] = lfo.Config();
  config.dry = dry;
  config.wet = wet;
}

fridge::config::Config& operator|=(fridge::config::Config& config,
                                   const UI& ui) {
  ui.UpdateConfig(config);
  return config;
};

}  // namespace fridge::ui
