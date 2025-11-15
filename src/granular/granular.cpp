#include <cstddef>
#include <optional>

#include "daisy_seed.h"
#include "daisysp.h"
#include "hid/logger.h"
#include "libjazz/slab.hpp"
#include "libjazz/value.hpp"

using namespace daisy;
using namespace daisysp;

DaisySeed hw;

struct Update {
  enum Kind { kErase, kWrite } kind;
  size_t finished_at;
  float value;
  size_t samples;
  std::optional<Update *> next;
};

static Slab<Update, 2048> UPDATES;

#define BUFFER_LEN 44100 * 2 /* 2 seconds */
#define MAX_FADE_TIME 10000

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
  float asSample() const { return getFloat(); }

public:
  BufferValue(float sample) : Value(sample) {}
  BufferValue() : BufferValue(0.0f) {}

  float sample() const {
    if (isPointer()) {
      return asSampleWithUpdates()->sample;
    } else {
      return asSample();
    }
  }

  Update *PushBack(Update &&update) {
    if (isPointer()) {
      Update *cur = asSampleWithUpdates()->first_update;
      while (!cur->next.has_value()) {
        cur = cur->next.value();
      }
      cur->next = UPDATES.Alloc(std::move(update));
      return cur->next.value();
    } else {
      float sample = asSample();
      auto upd = UPDATES.Alloc(std::move(update));
      auto head = SAMPLES.Alloc(sample, upd);
      new (this) Value((void *)head);
      return upd;
    }
  }
};

static BufferValue buffer[BUFFER_LEN];

void Erase(size_t index, size_t clock_time, float value,
           size_t samples = MAX_FADE_TIME) {
  buffer[index].PushBack(
      {Update::Kind::kErase, clock_time + samples, value, samples});

  // TODO
  // if index not in indices_to_update[clock_time % max_fade_time]:
  //     indices_to_update[clock_time % max_fade_time].append(index)
}

void Write(size_t index, size_t clock_time, float value,
           size_t samples = MAX_FADE_TIME) {
  buffer[index].PushBack({Update::Kind::kWrite, clock_time + samples, value,
                          static_cast<size_t>(value / samples)});

  // TODO
  // if index not in indices_to_update[clock_time % max_fade_time]:
  //     indices_to_update[clock_time % max_fade_time].append(index)
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
