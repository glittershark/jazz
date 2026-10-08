#include <algorithm>
#include <array>
#include <memory>
#include <random>
#include <vector>

#include "gtest/gtest.h"
#include "sound.hpp"

using namespace fridge;
using jazz::audio::StereoSample;

namespace {
struct Event {
  size_t position;
  uint32_t issued;
  float write;
  float erase;
};

// Independent, deliberately slow reference: an event ledger over 31 samples.
// It does not use the production slabs, tagged samples or housekeeping wheel.
class TapeModel {
 public:
  float Tick(size_t position, float write, float erase) {
    std::array<bool, 31> pending{};
    for (const auto& event : events_) {
      if (clock_ - event.issued == kFadeTime) {
        tape_[event.position] =
            tape_[event.position] * event.erase + event.write;
      } else {
        pending[event.position] = true;
      }
    }
    std::erase_if(events_, [&](const Event& event) {
      return clock_ - event.issued == kFadeTime;
    });
    for (size_t i = 0; i < tape_.size(); ++i) {
      if (!pending[i]) {
        tape_[i] = std::clamp(tape_[i], -1.0f, 1.0f);
      }
    }
    float gain = 1, addition = 0;
    for (const auto& event : events_) {
      if (event.position != position) {
        continue;
      }
      const float age = clock_ - event.issued;
      if (event.erase < 1) {
        const float offset = kFadeTime * event.erase / (1 - event.erase);
        gain *= offset / (age + offset + 1);
      }
      addition += event.write * age / kFadeTime;
    }
    const float output = tape_[position] * gain + addition;
    if (write != 0 || erase < 1) {
      events_.push_back({position, clock_, write, erase});
    }
    ++clock_;
    return output;
  }

 private:
  uint32_t clock_ = 0;
  std::array<float, 31> tape_{};
  std::vector<Event> events_;
};
}  // namespace

class AudioModelTest : public testing::TestWithParam<uint32_t> {};

TEST_P(AudioModelTest, RandomWritesErasesAndReadsMatchIndependentEventLedger) {
  auto sound = std::make_unique<sound::Sound>();
  TapeModel model;
  std::mt19937 rng(GetParam());
  mod::Frame frame;
  frame.head_count = 1;
  frame.dry = 0;
  frame.heads[0].read_amount = 1;
  for (size_t sample = 0; sample < 12000; ++sample) {
    const size_t position = rng() % 31;
    const bool writing = rng() % 3 != 0;
    const float input = (static_cast<int>(rng() % 61) - 30) / 1000.0f;
    const float erase = rng() % 3 == 0 ? (rng() % 11) / 10.0f : 1.0f;
    frame.heads[0].position = position;
    frame.heads[0].write_amount = writing ? 1 : 0;
    frame.heads[0].erase_amount = erase;
    const float expected = model.Tick(position, writing ? input : 0, erase);
    const auto actual =
        sound->ProcessSample(frame, StereoSample::OfMono(input));
    ASSERT_NEAR(actual.left, expected, 2e-6f) << "sample=" << sample;
    ASSERT_NEAR(actual.right, expected, 2e-6f) << "sample=" << sample;
  }
}

INSTANTIATE_TEST_SUITE_P(Seeds, AudioModelTest,
                         testing::Values(0u, 1u, 17u, 127u, 1234u, 98765u,
                                         0x80000000u, UINT32_MAX));
