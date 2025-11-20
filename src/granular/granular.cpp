#include <bitset>
#include <cstddef>
#include <iterator>
#include <optional>

#include "daisy_seed.h"
#include "daisysp.h"
#include "hid/logger.h"
#include "libjazz/slab.hpp"
#include "libjazz/value.hpp"

using namespace daisy;
using namespace daisysp;

DaisySeed hw;

constexpr const size_t BUFFER_LEN = 44100 * 2; /* 2 seconds */
constexpr const size_t MAX_FADE_TIME = 10000;
constexpr const size_t UPDATE_CAP = 2048;

struct Update {
  enum Kind { kErase, kWrite } kind;
  size_t finished_at;
  float value;
  size_t samples;
  Update *next;

  class Iterator {
    const Update *m_cur;

  public:
    using difference_type = std::ptrdiff_t;
    using value_type = Update;

    Iterator(const Update *cur) : m_cur(cur) {}

    const Update &operator*() const { return *m_cur; }
    void operator++(int) { ++*this; };
    Iterator &operator++() {
      m_cur = m_cur->next;
      return *this;
    };

    bool operator!=(Iterator &other) const { return other.m_cur != m_cur; }
  };
  friend Iterator;

  Iterator begin() const { return Iterator(this); }
  Iterator end() const { return Iterator(nullptr); }

  static_assert(std::input_iterator<Iterator>);
};

static Slab<Update, UPDATE_CAP> UPDATES;

class BufferValue : private Value {
public:
  struct SampleWithUpdates {
    float sample;
    Update *first_update;
  };

private:
  static Slab<SampleWithUpdates, BUFFER_LEN> SAMPLES;

  SampleWithUpdates *asSampleWithUpdates() const {
    return std::bit_cast<SampleWithUpdates *>(getPointer());
  }
  float &asSample() { return getFloat(); }

public:
  BufferValue(float sample) : Value(sample) {}
  BufferValue() : BufferValue(0.0f) {}

  float &sample() {
    if (isPointer()) {
      return asSampleWithUpdates()->sample;
    } else {
      return asSample();
    }
  }

  Update *PushBack(Update &&update) {
    if (isPointer()) {
      Update *cur = asSampleWithUpdates()->first_update;
      while (cur->next != nullptr) {
        cur = cur->next;
      }
      cur->next = UPDATES.Alloc(std::move(update));
      return cur->next;
    } else {
      float sample = asSample();
      auto upd = UPDATES.Alloc(std::move(update));
      auto head = SAMPLES.Alloc(sample, upd);
      new (this) Value((void *)head);
      return upd;
    }
  }

  Update **FirstUpdate() const {
    if (isPointer()) {
      return &asSampleWithUpdates()->first_update;
    } else {
      return nullptr;
    }
  }
};

class IndicesToUpdate {
private:
  size_t m_index;
  IndicesToUpdate *next;
  static Slab<IndicesToUpdate, UPDATE_CAP> SLAB;

public:
  IndicesToUpdate(size_t index) : m_index(index) {}

  static void Prepend(IndicesToUpdate **head, size_t index) {
    auto new_head = SLAB.Alloc(index);
    new_head->next = *head;
    *head = new_head;
  }

  size_t index() const { return m_index; }

  class Iterator {
    const IndicesToUpdate *m_cur;

  public:
    using difference_type = std::ptrdiff_t;
    using value_type = IndicesToUpdate;

    Iterator(const IndicesToUpdate *cur) : m_cur(cur) {}

    const IndicesToUpdate &operator*() const { return *m_cur; }
    void operator++(int) { ++*this; };
    Iterator &operator++() {
      m_cur = m_cur->next;
      return *this;
    };

    bool operator!=(Iterator &other) const { return other.m_cur != m_cur; }
  };
  friend Iterator;

  Iterator begin() const { return Iterator(this); }
  Iterator end() const { return Iterator(nullptr); }

  static_assert(std::input_iterator<Iterator>);
};

/// *** State ***

static BufferValue BUFFER[BUFFER_LEN];
static IndicesToUpdate *INDICES_TO_UPDATE[MAX_FADE_TIME];
static size_t global_clock_max = 300000;

void Erase(size_t index, size_t clock_time, float value,
           size_t samples = MAX_FADE_TIME) {
  BUFFER[index].PushBack(
      {Update::Kind::kErase, clock_time + samples, value, samples});
  IndicesToUpdate::Prepend(&INDICES_TO_UPDATE[clock_time % MAX_FADE_TIME],
                           index);
}

void Write(size_t index, size_t clock_time, float value,
           size_t samples = MAX_FADE_TIME) {
  BUFFER[index].PushBack({Update::Kind::kWrite, clock_time + samples, value,
                          static_cast<size_t>(value / samples)});
  IndicesToUpdate::Prepend(&INDICES_TO_UPDATE[clock_time % MAX_FADE_TIME],
                           index);
}

void DoUpdate(size_t index, size_t clock_time) {
  auto content = BUFFER[index];

  // Apply erases
  auto update_ptr = content.FirstUpdate();
  while (update_ptr != nullptr && *update_ptr != nullptr) {
    auto update = *update_ptr;
    if (update->kind == Update::Kind::kErase) {
      auto time_till_ripe =
          (update->finished_at - clock_time) % global_clock_max;
      if (time_till_ripe <= 0) {
        *update_ptr = update->next;
        UPDATES.Free(update);
        content.sample() *= update->value;
      }
    }
    update_ptr = &update->next;
  }

  // Apply writes
  update_ptr = content.FirstUpdate();
  while (update_ptr != nullptr && *update_ptr != nullptr) {
    auto update = *update_ptr;
    if (update->kind == Update::Kind::kWrite) {
      auto time_till_ripe =
          (update->finished_at - clock_time) % global_clock_max;
      if (time_till_ripe <= 0) {
        *update_ptr = update->next;
        UPDATES.Free(update);
        content.sample() += update->value;
      }
    }
    update_ptr = &update->next;
  }
}

float Read(size_t index, size_t clock_time) {
  auto content = BUFFER[index];
  auto value = content.sample();

  // Apply erases
  auto update = *content.FirstUpdate();
  while (update != nullptr) {
    if (update->kind == Update::Kind::kErase) {
      auto time_till_ripe =
          (update->finished_at - clock_time) % global_clock_max;
      auto frac_offset =
          (update->samples * update->value) / (1 - update->value);
      auto factor =
          frac_offset / ((update->samples - time_till_ripe) + frac_offset + 1);
      value *= factor;
    }
    update = update->next;
  }

  // Apply writes
  update = *content.FirstUpdate();
  while (update != nullptr) {
    if (update->kind == Update::Kind::kWrite) {
      auto time_till_ripe =
          (update->finished_at - clock_time) % global_clock_max;
      auto target_val = update->value;
      auto fading_in_val = target_val - (time_till_ripe * update->samples);
      value += fading_in_val;
    }
    update = update->next;
  }

  return value;
}

void PreHousekeeping(size_t clock_time) {
  for (auto &&index : *INDICES_TO_UPDATE[clock_time % MAX_FADE_TIME]) {
    DoUpdate(index.index(), clock_time);
  }
}

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out,
                   size_t size) {
  for (size_t i = 0; i < size; i++) {
    out[0][i] = in[0][i];
  }
}

int main(void) {

  hw.Configure();
  hw.Init();
  hw.SetAudioBlockSize(4);

  AdcChannelConfig adcConfig[2];
  adcConfig[0].InitSingle(hw.GetPin(21)); /* delay amount */
  adcConfig[1].InitSingle(hw.GetPin(20)); /* feedback */
  hw.adc.Init(adcConfig, 2);
  hw.adc.Start();

  hw.StartAudio(AudioCallback);
  for (;;) {
  }
}
