#include <array>
#include <cstddef>
#include <iostream>
#include <optional>
#include <random>

#include "granular.hpp"
#include "libjazz/slab.hpp"
#include "libjazz/value.hpp"
#include "system.h"

#ifndef UNIT_TEST
#include "daisy_seed.h"
#include "daisysp.h"
#include "hid/logger.h"

using namespace daisy;
using namespace daisysp;

DaisySeed hw;
#endif

Slab<BufferValue::SampleWithUpdates, UPDATE_CAP> BufferValue::SAMPLES;
Slab<IndicesToUpdate, UPDATE_CAP> IndicesToUpdate::SLAB;

Update *BufferValue::PushBack(Update &&update) {
  if (isSampleWithUpdates()) {
    auto head = asSampleWithUpdates();

    if (head->first_update == nullptr) {
      head->first_update = UPDATES.Alloc(std::move(update));
      return head->first_update;
    } else {
      Update *cur = asSampleWithUpdates()->first_update;
      while (cur->next_ != nullptr) {
        cur = cur->next_;
      }
      cur->next_ = UPDATES.Alloc(std::move(update));

      return cur->next_;
    }
  } else {
    float sample = asSample();
    auto upd = UPDATES.Alloc(std::move(update));
    decltype(SAMPLES)::Ptr<&SAMPLES> head =
        SAMPLES.AllocPtr<&SAMPLES>(sample, upd);
    new (this) BufferValue(head);
    return upd;
  }
}

Update **BufferValue::FirstUpdate() {
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
    for (auto &&_ : DrainingIterator(&*sample_with_updates)) {
    }
    SAMPLES.FreePtr(asSampleWithUpdates());
    new (this) BufferValue(sample_);
  }
}

// class Granular

void Granular::Erase(size_t index, size_t clock_time, float value,
                     size_t samples) {
  BUFFER[index].PushBack({.kind = Update::Kind::kErase,
                          .finished_at = clock_time + samples,
                          .value = value,
                          .samples = samples});
  IndicesToUpdate::Prepend(
      &indices_to_update_[(clock_time + samples) % MAX_FADE_TIME], index);
}

void Granular::Write(size_t index, size_t clock_time, float value,
                     size_t samples) {
  BUFFER[index].PushBack({.kind = Update::Kind::kWrite,
                          .finished_at = clock_time + samples,
                          .value = value,
                          .samples = samples});
  IndicesToUpdate::Prepend(
      &indices_to_update_[(clock_time + samples) % MAX_FADE_TIME], index);
}

void Granular::DoUpdate(size_t index, size_t clock_time) {
  auto content = &BUFFER[index];

  // Apply erases
  auto update_ptr = content->FirstUpdate();
  while (update_ptr != nullptr && *update_ptr != nullptr) {
    auto update = *update_ptr;
    if (update->kind == Update::Kind::kErase) {
      auto time_till_ripe =
          (update->finished_at - clock_time) % global_clock_max_;
      if (time_till_ripe == 0) {
        content->setSample(content->sample() * update->value);
        *update_ptr = update->next_;
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
          ((update->finished_at - clock_time) + global_clock_max_) %
          global_clock_max_;
      if (time_till_ripe == 0) {
        *update_ptr = update->next_;
        UPDATES.Free(update);
        content->setSample(content->sample() + update->value);
        continue;
      }
    }
    update_ptr = &update->next_;
  }

  content->Housekeep();
}

float Granular::Read(size_t index, size_t clock_time) {
  auto content = &BUFFER[index];
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
          ((update->finished_at - clock_time) + global_clock_max_) %
          global_clock_max_;
      auto frac_offset =
          (update->samples * update->value) / (1 - update->value);
      auto factor =
          frac_offset / ((update->samples - time_till_ripe) + frac_offset + 1);
      value *= factor;
    }
    update = update->next_;
  }

  // Apply writes
  update = *content->FirstUpdate();
  while (update != nullptr) {
    if (update->kind == Update::Kind::kWrite) {
      auto time_till_ripe =
          ((update->finished_at - clock_time) + global_clock_max_) %
          global_clock_max_;
      auto target_val = update->value;
      auto fading_in_val =
          target_val * (update->samples - time_till_ripe) / update->samples;
      value += fading_in_val;
    }
    update = update->next_;
  }

  return value;
}

void Granular::PreHousekeeping(size_t clock_time) {
  auto indices_to_update = indices_to_update_[clock_time % MAX_FADE_TIME];
  indices_to_update_[clock_time % MAX_FADE_TIME] = nullptr;

  if (indices_to_update == nullptr) {
    return;
  }

  for (auto &&index : indices_to_update->drain()) {
    DoUpdate(index.index(), clock_time);
  }
}

static std::array<Head, NUM_HEADS> heads{{
    {
        .kind = Head::Kind::kWrite,
        .direction = Head::Direction::kBackwards,
        .index = 0,
        .value = 1.0f,
        .step = 1,
    },
    {
        .kind = Head::Kind::kRead,
        .direction = Head::Direction::kForwards,
        .index = 10000,
        .value = 0.7f,
        .step = 1,
        .random = {{.grain_size = 10000}},
    },
    {
        .kind = Head::Kind::kRead,
        .direction = Head::Direction::kBackwards,
        .index = 20000,
        .value = 0.7f,
        .step = 2,
        .random = {{.grain_size = 20000}},
    },
    {
        .kind = Head::Kind::kErase,
        .direction = Head::Direction::kBackwards,
        .index = BUFFER_LEN / 2,
        .value = 0.5f,
        // .random = {{.grain_size = 20000}},
    },
}};

void Head::Process(float sample) {
  if (kind == Head::Kind::kWrite) {
    value = sample;
  }

  if (random) {
    if ((random->grain_remaining -= step) == 0) {
      random->fade = {
          .countdown = MAX_FADE_TIME,
          .old_index = index,
      };
      index = std::rand() % BUFFER_LEN;
      random->grain_remaining = random->grain_size;
      return;
    }
  }

  size_t step_by;
  switch (direction) {
  case Head::Direction::kForwards: {
    step_by = step;
    break;
  }
  case Head::Direction::kBackwards: {
    step_by = -step;
    break;
  }
  }

  index = (BUFFER_LEN + index + step_by) % BUFFER_LEN;
  if (random->fade) {
    if (random->fade->countdown-- == 0) {
      random->fade = std::nullopt;
    }
    random->fade->old_index =
        (BUFFER_LEN + random->fade->old_index + step_by) % BUFFER_LEN;
  }
}

#ifndef UNIT_TEST
static Granular granular;

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out,
                   size_t size) {
  for (size_t i = 0; i < size; i++) {
    auto sample = in[0][i];

    for (auto &&head : heads) {
      head.Process(sample);
    }

    out[0][i] = sample + granular.FullCycle(heads);
  }
}
#endif

#ifndef UNIT_TEST
int main(void) {
  hw.Configure();
  hw.Init();
  hw.SetAudioBlockSize(16);

  hw.StartAudio(AudioCallback);
  for (;;) {
  }
}
#endif
