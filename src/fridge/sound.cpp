#include "sound.hpp"

#include "config.hpp"
#include "constants.hpp"

namespace fridge::sound {

Slab<BufferValue::SampleWithUpdates, UPDATE_CAP> BufferValue::SAMPLES;
Slab<IndicesToUpdate, UPDATE_CAP> IndicesToUpdate::SLAB;

Update* BufferValue::PushBack(Update&& update) {
  if (isSampleWithUpdates()) {
    auto head = asSampleWithUpdates();

    if (head->first_update == nullptr) {
      head->first_update = UPDATES.Alloc(update);
      return head->first_update;
    } else {
      Update* cur = asSampleWithUpdates()->first_update;
      while (cur->next_ != nullptr) {
        cur = cur->next_;
      }
      cur->next_ = UPDATES.Alloc(update);

      return cur->next_;
    }
  } else {
    float sample = asSample();
    auto upd = UPDATES.Alloc(update);
    decltype(SAMPLES)::Ptr<&SAMPLES> head =
        SAMPLES.AllocPtr<&SAMPLES>(sample, upd);
    new (this) BufferValue(head);
    return upd;
  }
}

Update** BufferValue::FirstUpdate() {
  if (isSampleWithUpdates()) {
    return &asSampleWithUpdates()->first_update;
  } else {
    return nullptr;
  }
}

void BufferValue::Housekeep() {
  if (isSampleWithUpdates() && asSampleWithUpdates()->first_update == nullptr) {
    auto sample_ = sample();
    auto sample_with_updates = asSampleWithUpdates();
    for (auto&& _ : DrainingIterator(&*sample_with_updates)) {
    }
    SAMPLES.FreePtr(asSampleWithUpdates());
    new (this) BufferValue(sample_);
  }
}

// class Sound

float Sound::Read(size_t position) {
  auto content = &buffer_[position];
  auto value = content->sample();

  // Apply erases
  auto maybe_update = content->FirstUpdate();
  if (maybe_update == nullptr) {
    return value;
  }
  auto update = *maybe_update;

  while (update != nullptr) {
    if (update->kind == Update::Kind::kErase) {
      auto time_till_ripe =
          ((update->finished_at - global_clock_) + global_clock_max_) %
          global_clock_max_;
      auto frac_offset = (FADE_TIME * update->value) / (1 - update->value);
      auto factor =
          frac_offset / ((FADE_TIME - time_till_ripe) + frac_offset + 1);
      value *= factor;
    }
    update = update->next_;
  }

  // Apply writes
  update = *content->FirstUpdate();
  while (update != nullptr) {
    if (update->kind == Update::Kind::kWrite) {
      auto time_till_ripe =
          ((update->finished_at - global_clock_) + global_clock_max_) %
          global_clock_max_;
      auto target_val = update->value;
      auto fading_in_val =
          target_val * (FADE_TIME - time_till_ripe) / FADE_TIME;
      value += fading_in_val;
    }
    update = update->next_;
  }

  return value;
}

void Sound::Write(size_t position, float sample) {
  buffer_[position].PushBack({
      .kind = Update::Kind::kWrite,
      .finished_at = global_clock_ + FADE_TIME,
      .value = sample,
  });
  IndicesToUpdate::Prepend(
      &indices_to_update_[(global_clock_ + FADE_TIME) % FADE_TIME], position);
}

void Sound::Erase(size_t position, float amount) {
  buffer_[position].PushBack({
      .kind = Update::Kind::kErase,
      .finished_at = global_clock_ + FADE_TIME,
      .value = amount,
  });
  IndicesToUpdate::Prepend(
      &indices_to_update_[(global_clock_ + FADE_TIME) % FADE_TIME], position);
}

float Sound::ProcessSample(fridge::config::Config& config, float sample) {
  PreHousekeeping(global_clock_);

  float wet_signal = 0.f;

  for (auto&& head : config.heads()) {
    if (head.read_amount() > 0.f) {
      auto value = Read(head.position());
      wet_signal += value * head.read_amount();
    }

    if (head.write_amount() > 0.f) {
      Write(head.position(), sample * head.write_amount());
    }

    if (head.erase_amount() > 0.f) {
      Erase(head.position(), head.erase_amount());
    }
  }

  global_clock_ = (global_clock_ + 1) % global_clock_max_;

  return sample * config.dry() + wet_signal * config.wet();
}

}  // namespace fridge::sound
