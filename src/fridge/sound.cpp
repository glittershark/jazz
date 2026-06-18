#include "sound.hpp"

#include "config.hpp"
#include "constants.hpp"

namespace fridge::sound {

Slab<BufferValue::SampleWithUpdates, UPDATE_CAP> BufferValue::SAMPLES;
Slab<IndicesToUpdate, UPDATE_CAP> IndicesToUpdate::SLAB;

BufferValue::~BufferValue() {
  if (isSampleWithUpdates()) {
    auto head = asSampleWithUpdates();
    auto cur = head->first_update;
    while (cur != nullptr) {
      auto next = cur->next_;
      UPDATES.Free(cur);
      cur = next;
    }
    SAMPLES.FreePtr(asSampleWithUpdates());
  }
}

Update* BufferValue::PushBack(Update&& update) {
  if (isSampleWithUpdates()) {
    auto head = asSampleWithUpdates();

    // For erases, merge into the single pending erase rather than appending.
    // Multiplying the values is equivalent to applying both erases in sequence,
    // so the final result is the same.
    if (update.kind == Update::Kind::kErase && head->erase_update != nullptr) {
      head->erase_update->value *= update.value;
      return head->erase_update;
    }

    // Append via tail pointer — O(1).
    auto new_update = UPDATES.Alloc(update);
    if (head->last_update == nullptr) {
      head->first_update = new_update;
    } else {
      head->last_update->next_ = new_update;
    }
    head->last_update = new_update;
    if (update.kind == Update::Kind::kErase) {
      head->erase_update = new_update;
    }
    return new_update;
  } else {
    float sample = asSample();
    auto upd = UPDATES.Alloc(update);
    decltype(SAMPLES)::Ptr<&SAMPLES> head =
        SAMPLES.AllocPtr<&SAMPLES>(sample, upd);
    new (this) BufferValue(head);
    auto h = asSampleWithUpdates();
    // last_update is set by the SampleWithUpdates constructor; set
    // erase_update.
    if (update.kind == Update::Kind::kErase) {
      h->erase_update = upd;
    }
    return upd;
  }
}

void BufferValue::OnUpdateFreed(Update* freed_update) {
  if (!isSampleWithUpdates()) {
    return;
  }
  auto head = asSampleWithUpdates();

  if (freed_update == head->erase_update) {
    head->erase_update = nullptr;
  }

  if (freed_update == head->last_update) {
    // Freed the tail — scan for the new last node.
    Update* last = nullptr;
    for (Update* cur = head->first_update; cur != nullptr; cur = cur->next_) {
      last = cur;
    }
    head->last_update = last;
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

Sound::~Sound() {
  for (auto& ptr : indices_to_update_) {
    if (ptr != nullptr) {
      for (auto&& _ : ptr->drain()) {
      }
    }
  }
}

void Sound::DoUpdate(size_t index) {
  auto content = &buffer_[index];

  // Apply erases
  auto update_ptr = content->FirstUpdate();
  while (update_ptr != nullptr && *update_ptr != nullptr) {
    auto update = *update_ptr;
    if (update->kind == Update::Kind::kErase) {
      auto time_till_ripe =
          (update->finished_at - global_clock_) % global_clock_max_;
      if (time_till_ripe == 0) {
        content->setSample(content->sample() * update->value);
        *update_ptr = update->next_;
        content->OnUpdateFreed(update);
        UPDATES.Free(update);
        continue;
      }
    }
    update_ptr = &update->next_;
  }

  // Apply writes
  update_ptr = content->FirstUpdate();
  while (update_ptr != nullptr && *update_ptr != nullptr) {
    auto update = *update_ptr;
    if (update->kind == Update::Kind::kWrite) {
      auto time_till_ripe =
          ((update->finished_at - global_clock_) + global_clock_max_) %
          global_clock_max_;
      if (time_till_ripe == 0) {
        *update_ptr = update->next_;
        content->OnUpdateFreed(update);
        UPDATES.Free(update);
        content->setSample(content->sample() + update->value);
        continue;
      }
    }
    update_ptr = &update->next_;
  }

  content->Housekeep();
}

void Sound::PreHousekeeping(size_t clock_time) {
  auto indices_to_update = indices_to_update_[clock_time % FADE_TIME];
  indices_to_update_[clock_time % FADE_TIME] = nullptr;

  if (indices_to_update == nullptr) {
    return;
  }

  for (auto&& index : indices_to_update->drain()) {
    DoUpdate(index.index());
  }
}

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

float Sound::ApplyHead(const fridge::config::Head& head, float sample) {
  float wet_signal = 0.f;

  if (head.read_amount > 0.f) {
    auto value = Read(head.position);
    wet_signal = value * head.read_amount;
  }

  if (head.write_amount > 0.f) {
    Write(head.position, sample * head.write_amount);
  }

  if (head.erase_amount < 1.f) {
    Erase(head.position, head.erase_amount);
  }

  return wet_signal;
}

float Sound::ProcessSample(const fridge::config::Config& config, float sample) {
  PreHousekeeping(global_clock_);

  float wet_signal = 0.f;

  for (auto&& head : config.heads) {
    wet_signal += ApplyHead(head, sample);
  }

  global_clock_ = (global_clock_ + 1) % global_clock_max_;

  return sample * config.dry + wet_signal * config.wet;
}

float Sound::ProcessSample(const fridge::transition::Frame& frame,
                           float sample) {
  PreHousekeeping(global_clock_);

  float wet_signal = 0.f;

  for (size_t i = 0; i < frame.head_count; ++i) {
    wet_signal += ApplyHead(frame.heads[i].head, sample);
  }

  global_clock_ = (global_clock_ + 1) % global_clock_max_;

  return sample * frame.dry + wet_signal * frame.wet;
}

}  // namespace fridge::sound
