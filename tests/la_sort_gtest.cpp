#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <limits>
#include <random>
#include <vector>

#include "gtest/gtest.h"
#include "la_sort.hpp"
#include "panel.hpp"

namespace la_sort {
namespace {
using jazz::audio::StereoSample;

// Independent oracle: sort the whole recent window again, as in the prototype.
float ReferenceTap(const std::deque<float>& history, const Config& config) {
  std::vector<float> sorted(history.begin(), history.end());
  std::sort(sorted.begin(), sorted.end());
  double sum = 0.0;
  double total = 0.0;
  for (size_t rank = 0; rank < sorted.size(); ++rank) {
    const double position =
        sorted.size() == 1 ? 0.0 : double(rank) / double(sorted.size() - 1);
    const double weight = std::exp(-config.weight_sharpness *
                                   std::abs(position - config.weight_center));
    sum += sorted[rank] * weight;
    total += weight;
  }
  return history.back() * config.dry + float(sum / total) * config.wet;
}

TEST(LaSort, MatchesPrototypeThroughStartupDuplicatesAndWraparound) {
  for (size_t length : {size_t{1}, size_t{2}, size_t{17}, size_t{64},
                        size_t{127}, kMaxLength}) {
    for (float center : {0.0f, 0.23f, 0.5f, 1.0f}) {
      for (float sharpness : {0.0f, 2.0f, kMaxSharpness}) {
        SCOPED_TRACE(::testing::Message()
                     << "length=" << length << " center=" << center
                     << " sharpness=" << sharpness);
        const Config config{.weight_center = center,
                            .weight_sharpness = sharpness,
                            .length = length,
                            .volume_compensation = 0.0f};
        Parameters parameters;
        ASSERT_TRUE(parameters.SetConfig(config));
        Effect effect;
        std::deque<float> left;
        std::deque<float> right;
        std::mt19937 random(47);
        std::uniform_int_distribution<int> samples(-8, 8);
        for (size_t frame = 0; frame < 2 * kMaxLength + 1; ++frame) {
          const StereoSample sample{samples(random) / 8.0f,
                                    samples(random) / 8.0f};
          left.push_back(sample.left);
          right.push_back(sample.right);
          if (left.size() > config.length) {
            left.pop_front();
            right.pop_front();
          }
          const auto actual = effect.ProcessSample(parameters, sample);
          ASSERT_NEAR(actual.left, ReferenceTap(left, config), 2e-6f);
          ASSERT_NEAR(actual.right, ReferenceTap(right, config), 2e-6f);
        }
      }
    }
  }
}

TEST(LaSort, PreservesHistoryAcrossParameterChangesAndDryOnlyProcessing) {
  Effect effect;
  Parameters parameters;
  Config config{.dry = 1.0f, .wet = 0.0f, .volume_compensation = 0.0f};
  ASSERT_TRUE(parameters.SetConfig(config));
  std::deque<float> history;
  for (size_t frame = 0; frame < 400; ++frame) {
    if (frame == 30 || frame == 200) {
      config = {.weight_center = 0.8f,
                .weight_sharpness = 17.0f,
                .dry = 0.2f,
                .wet = 0.6f,
                .volume_compensation = 0.0f};
      ASSERT_TRUE(parameters.SetConfig(config));
    }
    const float sample = std::sin(float(frame) * 0.17f);
    history.push_back(sample);
    if (history.size() > config.length) {
      history.pop_front();
    }
    const auto actual = effect.ProcessSample(parameters, {sample, 0.0f});
    ASSERT_NEAR(actual.left, ReferenceTap(history, config), 2e-6f);
    ASSERT_EQ(actual.right, 0.0f);
  }
}

TEST(LaSort, OppositeStereoChannelsDoNotCancelAndInstancesStayIndependent) {
  Parameters parameters;
  ASSERT_TRUE(parameters.SetConfig({.volume_compensation = 0.0f}));
  Effect active;
  Effect silent;
  for (size_t frame = 0; frame < 200; ++frame) {
    const auto actual = active.ProcessSample(parameters, {0.4f, -0.4f});
    EXPECT_NEAR(actual.left, 0.4f, 1e-6f);
    EXPECT_NEAR(actual.right, -0.4f, 1e-6f);
    EXPECT_EQ(silent.ProcessSample(parameters, {0.0f, 0.0f}),
              StereoSample::Zero());
  }
}

TEST(LaSort, LengthDefaultsTo64AndOneSampleIsIdentity) {
  Config config;
  EXPECT_EQ(config.length, 64u);
  EXPECT_EQ(config.volume_compensation, 1.0f);
  EXPECT_EQ(config.lfo_rate, 1.0f);
  EXPECT_EQ(config.lfo_depth, 0.0f);
  config.length = 1;
  Parameters parameters;
  ASSERT_TRUE(parameters.SetConfig(config));
  Effect effect;
  for (size_t frame = 0; frame < 600; ++frame) {
    const StereoSample sample{std::sin(float(frame)), std::cos(float(frame))};
    EXPECT_EQ(effect.ProcessSample(parameters, sample), sample);
  }
}

TEST(LaSortLfo, ShiftsTheWholeSineSweepInwardWithoutClippingItsShape) {
  struct Sweep {
    float center, depth, low, high;
  };
  for (const Sweep sweep :
       {Sweep{0.1f, 0.0f, 0.1f, 0.1f}, Sweep{0.5f, 0.4f, 0.3f, 0.7f},
        Sweep{0.1f, 0.4f, 0.0f, 0.4f}, Sweep{0.9f, 0.4f, 0.6f, 1.0f},
        Sweep{0.0f, 1.0f, 0.0f, 1.0f}, Sweep{1.0f, 1.0f, 0.0f, 1.0f}}) {
    CenterLfo lfo(100.0f);
    Config config{.weight_center = sweep.center, .lfo_depth = sweep.depth};
    const double midpoint = (sweep.low + sweep.high) / 2.0;
    const double radius = (sweep.high - sweep.low) / 2.0;
    for (size_t frame = 0; frame <= 200; ++frame) {
      const float actual = lfo.Process(config);
      EXPECT_GE(actual, sweep.low - 1e-6f);
      EXPECT_LE(actual, sweep.high + 1e-6f);
      EXPECT_NEAR(
          actual,
          midpoint + radius * std::sin(6.283185307179586 * frame / 100.0),
          3e-7);
    }
  }
}

TEST(LaSortLfo, RateIsInHzAtBothLimitsAndDifferentSampleRates) {
  for (float sample_rate : {44100.0f, 48000.0f, 96000.0f}) {
    for (float rate : {kMinLfoRate, kMaxLfoRate}) {
      CenterLfo lfo(sample_rate);
      Config config{.lfo_rate = rate, .lfo_depth = 1.0f};
      const size_t quarter = std::lround(double(sample_rate) / (4.0 * rate));
      for (size_t frame = 0; frame <= 4 * quarter; ++frame) {
        const float center = lfo.Process(config);
        if (frame % quarter == 0) {
          const double expected =
              0.5 + 0.5 * std::sin(6.283185307179586 * frame * double(rate) /
                                   sample_rate);
          EXPECT_NEAR(center, expected, 2e-6f);
        }
      }
    }
  }
}

TEST(LaSortLfo, ControlsPreserveFreeRunningPhaseAndZeroDepthIsExact) {
  CenterLfo lfo(100.0f);
  Config config{.weight_center = 0.123f};
  for (size_t frame = 0; frame < 25; ++frame) {
    EXPECT_EQ(lfo.Process(config), config.weight_center);
  }
  config.lfo_depth = 1.0f;
  config.lfo_rate = 2.0f;
  EXPECT_NEAR(lfo.Process(config), 1.0f, 1e-7f);
  for (size_t frame = 1; frame <= 25; ++frame) {
    if (frame == 7) {
      config.weight_center = 1.0f;
    }
    EXPECT_NEAR(lfo.Process(config),
                0.5 + 0.5 * std::sin(6.283185307179586 * (0.25 + frame * 0.02)),
                3e-7);
  }
}

TEST(LaSort, ModulationMatchesFullSortReferenceAndPreservesPhaseAcrossChanges) {
  for (size_t length : {size_t{1}, size_t{17}, size_t{64}, kMaxLength}) {
    Config config{.weight_center = 0.1f,
                  .weight_sharpness = 32.0f,
                  .length = length,
                  .volume_compensation = 0.0f,
                  .lfo_rate = 20.0f,
                  .lfo_depth = 1.0f};
    Parameters parameters;
    ASSERT_TRUE(parameters.SetConfig(config));
    Effect effect(44100.0f);
    std::deque<float> history;
    double phase = 0.0;
    for (size_t frame = 0; frame < 3000; ++frame) {
      if (frame == 777) {
        config.weight_center = 0.9f;
        config.lfo_depth = 0.4f;
        config.lfo_rate = 7.5f;
        ASSERT_TRUE(parameters.SetConfig(config));
      }
      const float sample = float(int((frame * 47) % 211) - 105) / 128.0f;
      history.push_back(sample);
      if (history.size() > length) {
        history.pop_front();
      }
      Config expected_config = config;
      const double radius = config.lfo_depth / 2.0;
      expected_config.weight_center =
          std::clamp(double(config.weight_center), radius, 1.0 - radius) +
          radius * std::sin(6.283185307179586 * phase);
      const float expected = ReferenceTap(history, expected_config);
      const auto actual = effect.ProcessSample(parameters, {sample, sample});
      ASSERT_NEAR(actual.left, expected, 3e-6f)
          << "length=" << length << " frame=" << frame;
      EXPECT_EQ(actual.left, actual.right);
      phase = std::fmod(phase + config.lfo_rate / 44100.0, 1.0);
    }
  }
}

TEST(LaSort, CompensationScalesOnlyWetAndFollowsStartupAndLiveControls) {
  Config config{.dry = 0.25f, .wet = 0.5f, .length = kMaxLength};
  Parameters parameters;
  ASSERT_TRUE(parameters.SetConfig(config));
  Effect effect;
  for (size_t count = 1; count <= kMaxLength; ++count) {
    const auto actual = effect.ProcessSample(parameters, {0.2f, -0.2f});
    if (count == 1) {
      EXPECT_NEAR(actual.left, 0.15f, 2e-6f);
      EXPECT_NEAR(actual.right, -0.15f, 2e-6f);
    } else if (count == 64) {
      EXPECT_NEAR(actual.left, 0.85f, 2e-5f);
      EXPECT_NEAR(actual.right, -0.85f, 2e-5f);
    } else if (count == 256) {
      EXPECT_NEAR(actual.left, 1.65f, 2e-5f);
      EXPECT_NEAR(actual.right, -1.65f, 2e-5f);
    } else if (count == 1024) {
      // No compressor or limiter: constant signals receive the same fixed gain.
      EXPECT_NEAR(actual.left, 3.25f, 2e-5f);
      EXPECT_NEAR(actual.right, -3.25f, 2e-5f);
    }
  }

  // Changing compensation reuses the stored weights and recorded audio.
  config.volume_compensation = 0.5f;
  ASSERT_TRUE(parameters.SetConfig(config));
  EXPECT_NEAR(effect.ProcessSample(parameters, {0.2f, -0.2f}).left, 1.70f,
              2e-5f);
  config.volume_compensation = 0.0f;
  ASSERT_TRUE(parameters.SetConfig(config));
  EXPECT_NEAR(effect.ProcessSample(parameters, {0.2f, -0.2f}).left, 0.15f,
              2e-6f);
  config.volume_compensation = 1.0f;
  config.length = 1;
  ASSERT_TRUE(parameters.SetConfig(config));
  EXPECT_NEAR(effect.ProcessSample(parameters, {0.2f, -0.2f}).left, 0.15f,
              2e-6f);

  config.length = kMaxLength;
  config.wet = 0.0f;
  ASSERT_TRUE(parameters.SetConfig(config));
  for (size_t frame = 0; frame < 2 * kMaxLength; ++frame) {
    const StereoSample sample{std::sin(float(frame)), std::cos(float(frame))};
    EXPECT_EQ(effect.ProcessSample(parameters, sample), sample * config.dry);
  }
}

TEST(LaSort, CompensationGainDoesNotReactToSignalLevelOrSilence) {
  Parameters compensated_parameters;
  Parameters raw_parameters;
  ASSERT_TRUE(compensated_parameters.SetConfig({.length = 1024}));
  ASSERT_TRUE(
      raw_parameters.SetConfig({.length = 1024, .volume_compensation = 0.0f}));
  Effect compensated;
  Effect raw;
  for (size_t frame = 0; frame < 8192; ++frame) {
    const float level = frame < 2048 ? 0.001f : frame < 4096 ? 0.75f : 0.0f;
    const StereoSample input{level * std::sin(float(frame) * 0.01f), level};
    const auto actual =
        compensated.ProcessSample(compensated_parameters, input);
    const auto original = raw.ProcessSample(raw_parameters, input);
    if (frame >= 1023) {
      EXPECT_FLOAT_EQ(actual.left, original.left * 32.0f);
      EXPECT_FLOAT_EQ(actual.right, original.right * 32.0f);
    }
  }
}

TEST(LaSort, FixedCompensationOffsetsAveragingLossAcrossLengths) {
  // Measure AC RMS, not just a pointwise copy of the gain formula. Reuse the
  // same deterministic broadband input at each length and omit startup.
  for (float sharpness : {0.0f, 2.0f, 12.0f}) {
    double reference_rms = 0.0;
    for (size_t length : {size_t{64}, size_t{256}, kMaxLength}) {
      SCOPED_TRACE(::testing::Message()
                   << "length=" << length << " sharpness=" << sharpness);
      Parameters parameters;
      ASSERT_TRUE(parameters.SetConfig(
          {.weight_sharpness = sharpness, .length = length}));
      Effect effect;
      std::mt19937 random(47);
      std::uniform_real_distribution<float> samples(-0.5f, 0.5f);
      constexpr size_t frames = 65536;
      double sum = 0.0;
      double power = 0.0;
      double input_power = 0.0;
      for (size_t frame = 0; frame < frames + kMaxLength; ++frame) {
        const float input = samples(random);
        const auto actual = effect.ProcessSample(parameters, {input, 0.0f});
        EXPECT_EQ(actual.right, 0.0f);
        if (frame >= kMaxLength) {
          sum += actual.left;
          power += double(actual.left) * actual.left;
          input_power += double(input) * input;
        }
      }
      const double mean = sum / frames;
      const double rms = std::sqrt(power / frames - mean * mean);
      if (sharpness == 0.0f) {
        EXPECT_NEAR(20.0 * std::log10(rms / std::sqrt(input_power / frames)),
                    0.0, 1.0);
      }
      if (length == 64) {
        reference_rms = rms;
      } else {
        EXPECT_NEAR(20.0 * std::log10(rms / reference_rms), 0.0, 1.0);
      }
    }
  }
}

TEST(LaSort,
     LiveLengthChangesRetainRecentAudioWithoutRevivingDiscardedSamples) {
  Parameters parameters;
  Effect effect;
  Config config{.volume_compensation = 0.0f};
  std::deque<float> left;
  std::deque<float> right;
  for (size_t frame = 0; frame < 1800; ++frame) {
    if (frame % 53 == 0) {
      const std::array<size_t, 6> lengths{kMaxLength, 3, 128, 1, 17, 64};
      config.length = lengths[(frame / 53) % lengths.size()];
    }
    if (frame % 97 == 0) {
      config.weight_center = float((frame / 97) % 5) / 4.0f;
      config.weight_sharpness = (frame / 97) % 2 ? 32.0f : 0.0f;
    }
    // Changing mix alone must reuse the same weighting curve safely.
    config.dry = float(frame % 3) / 4.0f;
    config.wet = 1.0f - config.dry;
    ASSERT_TRUE(parameters.SetConfig(config));
    const StereoSample sample{float(int(frame % 13) - 6) / 8.0f,
                              float(int(frame % 19) - 9) / 10.0f};
    left.push_back(sample.left);
    right.push_back(sample.right);
    while (left.size() > config.length) {
      left.pop_front();
      right.pop_front();
    }
    const auto actual = effect.ProcessSample(parameters, sample);
    ASSERT_NEAR(actual.left, ReferenceTap(left, config), 2e-6f) << frame;
    ASSERT_NEAR(actual.right, ReferenceTap(right, config), 2e-6f) << frame;
  }
}

TEST(LaSort, InvalidLengthsLeaveParametersUnchanged) {
  Parameters parameters;
  Parameters reference;
  Effect actual;
  Effect expected;
  for (size_t length :
       {size_t{0}, kMaxLength + 1, std::numeric_limits<size_t>::max()}) {
    Config config{.length = length};
    EXPECT_FALSE(IsValid(config));
    EXPECT_FALSE(parameters.SetConfig(config));
    for (size_t frame = 0; frame < 300; ++frame) {
      const StereoSample sample{std::sin(float(frame)), 0.2f};
      EXPECT_EQ(actual.ProcessSample(parameters, sample),
                expected.ProcessSample(reference, sample));
    }
  }
}

TEST(LaSort, RejectsInvalidSettingsWithoutChangingActiveWeights) {
  const std::array<float Config::*, 7> fields{
      &Config::weight_center, &Config::weight_sharpness,    &Config::dry,
      &Config::wet,           &Config::volume_compensation, &Config::lfo_rate,
      &Config::lfo_depth};
  Parameters parameters;
  Parameters unchanged;
  Effect actual;
  Effect expected;
  for (auto field : fields) {
    for (float value : {-1.0f, 33.0f, std::numeric_limits<float>::infinity(),
                        std::numeric_limits<float>::quiet_NaN()}) {
      Config invalid;
      invalid.*field = value;
      EXPECT_FALSE(IsValid(invalid));
      EXPECT_FALSE(parameters.SetConfig(invalid));
      for (size_t frame = 0; frame < 65; ++frame) {
        const StereoSample sample{float(frame) / 65.0f, -0.5f};
        EXPECT_EQ(actual.ProcessSample(parameters, sample),
                  expected.ProcessSample(unchanged, sample));
      }
    }
  }
}

TEST(LaSort, NonFiniteSamplesBecomeSilenceWithoutPoisoningHistory) {
  Parameters parameters;
  Effect effect;
  Effect reference;
  const std::array<float, 3> invalid{std::numeric_limits<float>::quiet_NaN(),
                                     std::numeric_limits<float>::infinity(),
                                     -std::numeric_limits<float>::infinity()};
  for (size_t frame = 0; frame < 512; ++frame) {
    const bool bad = frame < invalid.size();
    const float sample = bad ? invalid[frame] : 0.3f;
    EXPECT_EQ(effect.ProcessSample(parameters, {sample, -sample}),
              reference.ProcessSample(
                  parameters,
                  bad ? StereoSample::Zero() : StereoSample{sample, -sample}));
  }
}

TEST(LaSort, ModulationRangesRejectOutOfBoundsSettings) {
  Parameters parameters;
  for (float rate : {0.0f, 0.049f, 20.01f}) {
    EXPECT_FALSE(parameters.SetConfig({.lfo_rate = rate}));
  }
  for (float depth : {-0.001f, 1.001f}) {
    EXPECT_FALSE(parameters.SetConfig({.lfo_depth = depth}));
  }
  EXPECT_TRUE(parameters.SetConfig({.lfo_rate = 0.05f, .lfo_depth = 0.0f}));
  EXPECT_TRUE(parameters.SetConfig({.lfo_rate = 20.0f, .lfo_depth = 1.0f}));
}

TEST(LaSortPanel, FridgeKnobsControlTheCorrespondingParameters) {
  Config config;
  EXPECT_TRUE(ApplyKnobTicks(config, 0, -48));  // Wet, half a turn down.
  EXPECT_TRUE(ApplyKnobTicks(config, 1, 24));   // Dry, quarter turn up.
  EXPECT_TRUE(ApplyKnobTicks(config, 2, 12));   // Position becomes center.
  EXPECT_TRUE(ApplyKnobTicks(config, 3, 24));   // Write becomes sharpness.
  EXPECT_FLOAT_EQ(config.wet, 0.5f);
  EXPECT_FLOAT_EQ(config.dry, 0.25f);
  EXPECT_FLOAT_EQ(config.weight_center, 0.625f);
  EXPECT_FLOAT_EQ(config.weight_sharpness, 10.0f);
  EXPECT_TRUE(IsValid(config));

  // Fridge's wet/dry mux channels and LED columns have opposite ordering.
  EXPECT_EQ(kKnobs[0].led_column, 1);
  EXPECT_EQ(kKnobs[1].led_column, 0);
  EXPECT_EQ(kKnobs[2].led_column, 2);
  EXPECT_EQ(kKnobs[3].led_column, 3);
  EXPECT_TRUE(ApplyKnobTicks(config, 5, -48));
  EXPECT_FLOAT_EQ(config.lfo_rate, kMinLfoRate);
  EXPECT_TRUE(ApplyKnobTicks(config, 5, 48));
  EXPECT_NEAR(config.lfo_rate, 1.0f, 1e-6f);
  EXPECT_TRUE(ApplyKnobTicks(config, 6, 48));
  EXPECT_FLOAT_EQ(config.lfo_depth, 0.5f);
  EXPECT_EQ(kKnobs[5].led_column, 5);
  EXPECT_EQ(kKnobs[6].led_column, 6);
}

TEST(LaSortPanel, EndStopsClampAndIdleKnobsDoNotRequestUpdates) {
  Config config;
  for (size_t channel = 0; channel < kKnobs.size(); ++channel) {
    if (channel == kLengthChannel) {
      continue;
    }
    EXPECT_FALSE(ApplyKnobTicks(config, channel, 0));
    ApplyKnobTicks(config, channel, std::numeric_limits<int>::lowest());
    EXPECT_FLOAT_EQ(config.*kKnobs[channel].parameter, kKnobs[channel].minimum);
    EXPECT_FALSE(ApplyKnobTicks(config, channel, -1));
    EXPECT_TRUE(
        ApplyKnobTicks(config, channel, std::numeric_limits<int>::max()));
    EXPECT_FLOAT_EQ(config.*kKnobs[channel].parameter, kKnobs[channel].maximum);
    EXPECT_FALSE(ApplyKnobTicks(config, channel, 1));
    EXPECT_TRUE(IsValid(config));
  }
}

TEST(LaSortPanel, FullTurnCoversEachRangeAndReverses) {
  Config config{.weight_center = 0.0f,
                .weight_sharpness = 0.0f,
                .dry = 0.0f,
                .wet = 0.0f,
                .lfo_rate = kMinLfoRate};
  for (size_t channel = 0; channel < kKnobs.size(); ++channel) {
    if (channel == kLengthChannel) {
      continue;
    }
    for (int tick = 0; tick < kTicksPerTurn; ++tick) {
      EXPECT_TRUE(ApplyKnobTicks(config, channel, 1));
    }
    EXPECT_NEAR(config.*kKnobs[channel].parameter, kKnobs[channel].maximum,
                2e-5f);
    for (int tick = 0; tick < kTicksPerTurn; ++tick) {
      EXPECT_TRUE(ApplyKnobTicks(config, channel, -1));
    }
    EXPECT_NEAR(config.*kKnobs[channel].parameter, kKnobs[channel].minimum,
                2e-5f);
  }
}

TEST(LaSortPanel, RejectsUnknownChannelsAndInvalidConfigurations) {
  Config config;
  EXPECT_FALSE(ApplyKnobTicks(config, kKnobs.size(), 1));
  EXPECT_FALSE(ApplyKnobTicks(config, std::numeric_limits<size_t>::max(), 1));
  EXPECT_EQ(config.weight_center, 0.5f);
  EXPECT_EQ(config.weight_sharpness, 2.0f);
  EXPECT_EQ(config.dry, 0.0f);
  EXPECT_EQ(config.wet, 1.0f);

  config.weight_sharpness = std::numeric_limits<float>::quiet_NaN();
  EXPECT_FALSE(ApplyKnobTicks(config, 2, 24));
  EXPECT_EQ(config.weight_center, 0.5f);
}

TEST(LaSortPanel, ReadAmountKnobAdjustsLengthInWholeSamplesAndClamps) {
  Config config;
  EXPECT_FALSE(ApplyKnobTicks(config, 4, 0));
  EXPECT_TRUE(ApplyKnobTicks(config, 4, 1));
  EXPECT_EQ(config.length, 65u);
  EXPECT_TRUE(ApplyKnobTicks(config, 4, -1));
  EXPECT_EQ(config.length, 64u);
  EXPECT_TRUE(ApplyKnobTicks(config, 4, std::numeric_limits<int>::max()));
  EXPECT_EQ(config.length, kMaxLength);
  EXPECT_FALSE(ApplyKnobTicks(config, 4, 1));
  EXPECT_TRUE(ApplyKnobTicks(config, 4, std::numeric_limits<int>::lowest()));
  EXPECT_EQ(config.length, 1u);
  EXPECT_FALSE(ApplyKnobTicks(config, 4, -1));
  EXPECT_FLOAT_EQ(config.weight_center, 0.5f);
  EXPECT_FLOAT_EQ(config.weight_sharpness, 2.0f);
  EXPECT_FLOAT_EQ(config.dry, 0.0f);
  EXPECT_FLOAT_EQ(config.wet, 1.0f);
}

}  // namespace
}  // namespace la_sort
