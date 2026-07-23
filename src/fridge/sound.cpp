#include "sound.hpp"

#include "config.hpp"
#include "constants.hpp"
#include "libjazz/stereo_sample.hpp"

namespace fridge::sound {

using jazz::audio::StereoSample;

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

namespace {
inline float ComputeEraseFracOffset(float value) {
  return (FADE_TIME * value) / (1 - value);
}
}  // namespace

Update* BufferValue::PushBack(Update&& update) {
  if (update.kind == Update::Kind::kErase) {
    update.erase_frac_offset = ComputeEraseFracOffset(update.value);
  }

  if (isSampleWithUpdates()) {
    auto head = asSampleWithUpdates();

    // For erases, merge into the single pending erase rather than appending.
    // Multiplying the values is equivalent to applying both erases in sequence,
    // so the final result is the same.
    if (update.kind == Update::Kind::kErase && head->erase_update != nullptr) {
      head->erase_update->value *= update.value;
      head->erase_update->erase_frac_offset =
          ComputeEraseFracOffset(head->erase_update->value);
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
  for (auto buffer : {&left_buffer_, &right_buffer_}) {
    auto content = &((*buffer)[index]);

    // Single walk. The list is sorted by finished_at (updates are appended
    // at the tail with monotonically-increasing global_clock_+FADE_TIME
    // values; erase merges don't reorder), so once we see a non-ripe update
    // we're done. Erase-before-write ordering is preserved by deferring the
    // sample math until the walk completes.
    Update* ripe_erase = nullptr;
    float ripe_write_sum = 0.0f;
    auto have_ripe_writes = false;

    auto update_ptr = content->FirstUpdate();
    while (update_ptr != nullptr && *update_ptr != nullptr) {
      auto update = *update_ptr;
      if (update->finished_at > global_clock_) {
        break;
      }
      if (update->finished_at == global_clock_) {
        if (update->kind == Update::Kind::kErase) {
          ripe_erase = update;
        } else {
          ripe_write_sum += update->value;
          have_ripe_writes = true;
        }
        *update_ptr = update->next_;
        content->OnUpdateFreed(update);
        // Defer freeing the erase until we've applied its value.
        if (update->kind == Update::Kind::kWrite) {
          UPDATES.Free(update);
        }
        continue;
      }
      // finished_at < global_clock_ shouldn't happen (would mean we missed
      // a tick) — skip defensively.
      update_ptr = &update->next_;
    }

    if (ripe_erase != nullptr) {
      float new_sample = content->sample() * ripe_erase->value;
      if (have_ripe_writes) {
        new_sample += ripe_write_sum;
      }
      content->setSample(new_sample);
      UPDATES.Free(ripe_erase);
    } else if (have_ripe_writes) {
      content->setSample(content->sample() + ripe_write_sum);
    }

    content->Housekeep();
  }
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

StereoSample Sound::Read(size_t position) {
  auto read_channel = [&](auto buffer) {
    auto content = &((*buffer)[position]);
    auto value = content->sample();

    auto maybe_update = content->FirstUpdate();
    if (maybe_update == nullptr) {
      return value;
    }

    // Single walk: accumulate the (at-most-one) erase factor and the sum of
    // fading-in writes.
    //
    // `time_till_ripe` is bounded by FADE_TIME (DoUpdate removes updates when
    // ripe), so the subtraction is safe without a modulo.
    float erase_factor = 1.0f;
    float write_sum = 0.0f;
    for (auto update = *maybe_update; update != nullptr;
         update = update->next_) {
      auto time_till_ripe = update->finished_at - global_clock_;
      if (update->kind == Update::Kind::kErase) {
        // erase_frac_offset is precomputed in BufferValue::PushBack.
        erase_factor =
            update->erase_frac_offset /
            ((FADE_TIME - time_till_ripe) + update->erase_frac_offset + 1);
      } else {
        write_sum += update->value * (FADE_TIME - time_till_ripe) / FADE_TIME;
      }
    }

    return value * erase_factor + write_sum;
  };

  return StereoSample{
      .left = read_channel(&left_buffer_),
      .right = read_channel(&right_buffer_),
  };
}

void Sound::Write(size_t position, StereoSample sample) {
  auto do_write = [&](auto buffer, float sample) {
    (*buffer)[position].PushBack({
        .kind = Update::Kind::kWrite,
        .finished_at = global_clock_ + FADE_TIME,
        .value = sample,
    });
    IndicesToUpdate::Prepend(
        &indices_to_update_[(global_clock_ + FADE_TIME) % FADE_TIME], position);
  };

  do_write(&left_buffer_, sample.left);
  do_write(&right_buffer_, sample.right);
}

void Sound::Erase(size_t position, StereoSample amount) {
  auto do_erase = [&](auto buffer, float amount) {
    (*buffer)[position].PushBack({
        .kind = Update::Kind::kErase,
        .finished_at = global_clock_ + FADE_TIME,
        .value = amount,
    });
    IndicesToUpdate::Prepend(
        &indices_to_update_[(global_clock_ + FADE_TIME) % FADE_TIME], position);
  };

  do_erase(&left_buffer_, amount.left);
  do_erase(&right_buffer_, amount.right);
}

StereoSample Sound::ApplyHead(const fridge::config::Head& head,
                              StereoSample sample) {
  auto wet_signal = StereoSample::Zero();

  // Wrap once here so a position past the end of the tape can never index
  // out of the buffer.
  size_t position = head.position % BUFFER_LEN;

  if (head.read_amount > 0.f) {
    auto value = Read(position);
    wet_signal = value * head.ReadAmount();
  }

  if (head.write_amount > 0.f) {
    Write(position, sample * head.WriteAmount());
  }

  if (head.erase_amount < 1.f) {
    Erase(position, head.EraseAmount());
  }

  return wet_signal;
}

StereoSample Sound::ProcessSample(const fridge::config::Config& config,
                                  StereoSample sample) {
  PreHousekeeping(global_clock_);

  auto wet_signal = StereoSample::Zero();

  for (auto&& head : config.heads) {
    wet_signal += ApplyHead(head, sample);
  }

  global_clock_ = (global_clock_ + 1) % global_clock_max_;

  return sample * config.dry + wet_signal * config.wet;
}

StereoSample Sound::ProcessSample(const fridge::mod::Frame& frame,
                                  StereoSample sample) {
  PreHousekeeping(global_clock_);

  auto wet_signal = StereoSample::Zero();

  for (size_t i = 0; i < frame.head_count; ++i) {
    wet_signal += ApplyHead(frame.heads[i], sample);
  }

  global_clock_ = (global_clock_ + 1) % global_clock_max_;

  return sample * frame.dry + wet_signal * frame.wet;
}

}  // namespace fridge::sound
