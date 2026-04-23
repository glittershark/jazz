#include "ui.hpp"

#include "config.hpp"

namespace fridge::ui {

config::Head Head::Config() const {
  return {
      .position = position.Get(),
      .write_amount = write_amount.Get(),
      .read_amount = read_amount.Get(),
      .erase_amount = erase_amount.Get(),
      .feedback = feedback.Get(),
  };
};

void Head::Select(const fridge::config::Head& head) {
  position.Set(head.position);
  write_amount.Set(head.write_amount);
  read_amount.Set(head.read_amount);
  erase_amount.Set(head.erase_amount);
  feedback.Set(head.feedback);
}

fridge::config::LFO LFO::Config() const {
  return {
      .range = range.Get(),
      .max_grain_size = max_grain_size.Get(),
      .min_grain_size = min_grain_size.Get(),
      .reverse_chance = reverse_chance.Get(),
      .teleport_chance = teleport_chance.Get(),
      .pitch_shift_chance = pitch_shift_chance.Get(),
      .low_octave_chance = low_octave_chance.Get(),
      .high_octave_chance = high_octave_chance.Get(),
  };
}

void LFO::Select(const fridge::config::LFO& lfo) {
  range.Set(lfo.range);
  max_grain_size.Set(lfo.max_grain_size);
  min_grain_size.Set(lfo.min_grain_size);
  reverse_chance.Set(lfo.reverse_chance);
  teleport_chance.Set(lfo.teleport_chance);
  pitch_shift_chance.Set(lfo.pitch_shift_chance);
  low_octave_chance.Set(lfo.low_octave_chance);
  high_octave_chance.Set(lfo.high_octave_chance);
}

void fridge::ui::UI::UpdateConfig(config::Config& config) const {
  config.heads[selected_head] = head.Config();
  config.lfos[selected_lfo] = lfo.Config();
  config.dry = dry.Get();
  config.wet = wet.Get();
}

}  // namespace fridge::ui

fridge::config::Config& operator|=(fridge::config::Config& config,
                                   const fridge::ui::UI& ui) {
  ui.UpdateConfig(config);
  return config;
};
