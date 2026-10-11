#include "la_sort.hpp"

#include <algorithm>
#include <cmath>

namespace la_sort {
namespace {
float ValidSample(float sample);
float ValidSampleRate(float sample_rate);
}  // namespace

CenterLfo::CenterLfo(float sample_rate)
    : sample_period_(1.0 / ValidSampleRate(sample_rate)) {}

float CenterLfo::Process(const Config& config) {
  float center = config.weight_center;
  if (config.lfo_depth > 0.0f) {
    const float radius = 0.5f * config.lfo_depth;
    const float midpoint = std::clamp(center, radius, 1.0f - radius);
    center =
        midpoint + radius * std::sin(float(phase_ * 6.2831853071795864769));
    center = std::clamp(center, 0.0f, 1.0f);
  }
  // Preserve phase through control changes and while depth/wet are zero.
  phase_ += config.lfo_rate * sample_period_;
  phase_ -= std::floor(phase_);
  return center;
}

Parameters::Parameters() {
  SetConfig(Config{});
}

bool Parameters::SetConfig(const Config& config) {
  if (!IsValid(config)) {
    return false;
  }

  // Cache each startup size. Mixing and length-only changes reuse rows whose
  // weighting curve is unchanged; bulk preparation stays outside the audio path.
  if (config.weight_center != config_.weight_center ||
      config.weight_sharpness != config_.weight_sharpness) {
    prepared_length_ = 0;
  }
  for (size_t count = prepared_length_ + 1; count <= config.length; ++count) {
    const float target = config.weight_center * float(count - 1);
    const size_t pivot = static_cast<size_t>(target);
    const float slope =
        count == 1 ? 0.0f : config.weight_sharpness / float(count - 1);
    RankWeights weights{
        .left = std::exp(-slope * (target - float(pivot))),
        .right = pivot + 1 < count
                     ? std::exp(-slope * (float(pivot + 1) - target))
                     : 0.0f,
        // Store the small decrement accurately even when decay is near one.
        .decay_step = -std::expm1(-slope),
        // Fixed, signal-independent correction for the 1/sqrt(N) RMS loss
        // of averaging independent samples. Never measures audio level.
        .makeup_gain = std::sqrt(float(count)),
        .pivot = pivot,
    };
    // Include small tail weights without losing them in a long float sum.
    double total = 0.0;
    float weight = weights.left;
    for (size_t rank = pivot + 1; rank > 0; --rank) {
      total += weight;
      weight -= weight * weights.decay_step;
    }
    weight = weights.right;
    for (size_t rank = pivot + 1; rank < count; ++rank) {
      total += weight;
      weight -= weight * weights.decay_step;
    }
    weights.left /= total;
    weights.right /= total;
    weights_[count - 1] = weights;
  }
  prepared_length_ = std::max(prepared_length_, config.length);
  config_ = config;
  return true;
}

jazz::audio::StereoSample Effect::ProcessSample(
    const Parameters& parameters, jazz::audio::StereoSample sample) {
  sample = {ValidSample(sample.left), ValidSample(sample.right)};
  const size_t length = parameters.config_.length;
  if (count_ > length) {
    ShrinkWindow(length);
  }
  InsertSample(left_, sample.left, length);
  InsertSample(right_, sample.right, length);
  count_ = std::min(count_ + 1, length);
  write_index_ = (write_index_ + 1) % kMaxLength;

  const auto& config = parameters.config_;
  const float center = center_lfo_.Process(config);
  const auto wet = ReadWindow(parameters, center);
  const float gain =
      1.0f + config.volume_compensation *
                 (parameters.weights_[count_ - 1].makeup_gain - 1.0f);
  return sample * config.dry + wet * (config.wet * gain);
}

jazz::audio::StereoSample Effect::ReadWindow(const Parameters& parameters,
                                             float center) {
  auto weights = parameters.weights_[count_ - 1];
  const auto& config = parameters.config_;
  const bool modulated =
      config.lfo_depth > 0.0f && config.weight_sharpness > 0.0f && count_ > 1;
  if (modulated) {
    // Reuse the prepared decay; only the two starting weights depend on the
    // current center. Normalize during the rank walk, without rebuilding
    // tables.
    const float target = center * float(count_ - 1);
    const float slope = config.weight_sharpness / float(count_ - 1);
    weights.pivot = static_cast<size_t>(target);
    weights.left = std::exp(-slope * (target - float(weights.pivot)));
    weights.right = weights.pivot + 1 < count_
                        ? std::exp(-slope * (float(weights.pivot + 1) - target))
                        : 0.0f;
  }
  jazz::audio::StereoSample wet = jazz::audio::StereoSample::Zero();
  double total = 0.0;
  float weight = weights.left;
  for (size_t rank = weights.pivot + 1; rank > 0; --rank) {
    wet.left += left_.sorted[rank - 1] * weight;
    wet.right += right_.sorted[rank - 1] * weight;
    if (modulated) {
      total += weight;
    }
    weight -= weight * weights.decay_step;
  }
  weight = weights.right;
  for (size_t rank = weights.pivot + 1; rank < count_; ++rank) {
    wet.left += left_.sorted[rank] * weight;
    wet.right += right_.sorted[rank] * weight;
    if (modulated) {
      total += weight;
    }
    weight -= weight * weights.decay_step;
  }
  return modulated ? wet * float(1.0 / total) : wet;
}

void Effect::ShrinkWindow(size_t length) {
  count_ = length;
  const size_t oldest = (write_index_ + kMaxLength - count_) % kMaxLength;
  for (Channel* channel : {&left_, &right_}) {
    for (size_t i = 0; i < count_; ++i) {
      channel->sorted[i] = channel->history[(oldest + i) % kMaxLength];
    }
    std::sort(channel->sorted.begin(), channel->sorted.begin() + count_);
  }
}

void Effect::InsertSample(Channel& channel, float sample, size_t length) {
  size_t count = count_;
  if (count == length) {
    const float oldest =
        channel.history[(write_index_ + kMaxLength - count) % kMaxLength];
    const auto end = channel.sorted.begin() + count;
    const auto removed = std::lower_bound(channel.sorted.begin(), end, oldest);
    std::move(removed + 1, end, removed);
    --count;
  }

  const auto end = channel.sorted.begin() + count;
  const auto inserted = std::lower_bound(channel.sorted.begin(), end, sample);
  std::move_backward(inserted, end, end + 1);
  *inserted = sample;
  channel.history[write_index_] = sample;
}

// ---------------------------------------------------------------------------
// Validation
// ---------------------------------------------------------------------------

bool IsValid(const Config& config) {
  return config.length >= 1 && config.length <= kMaxLength &&
         std::isfinite(config.weight_center) && config.weight_center >= 0.0f &&
         config.weight_center <= 1.0f &&
         std::isfinite(config.weight_sharpness) &&
         config.weight_sharpness >= 0.0f &&
         config.weight_sharpness <= kMaxSharpness &&
         std::isfinite(config.dry) && config.dry >= 0.0f &&
         config.dry <= 1.0f && std::isfinite(config.wet) &&
         config.wet >= 0.0f && config.wet <= 1.0f &&
         std::isfinite(config.volume_compensation) &&
         config.volume_compensation >= 0.0f &&
         config.volume_compensation <= 1.0f && std::isfinite(config.lfo_rate) &&
         config.lfo_rate >= kMinLfoRate && config.lfo_rate <= kMaxLfoRate &&
         std::isfinite(config.lfo_depth) && config.lfo_depth >= 0.0f &&
         config.lfo_depth <= 1.0f;
}

namespace {
float ValidSampleRate(float sample_rate) {
  return std::isfinite(sample_rate) && sample_rate > 0.0f ? sample_rate
                                                          : 48000.0f;
}

float ValidSample(float sample) {
  // A NaN in the sorted history would break eviction of the oldest sample.
  return std::isfinite(sample) ? sample : 0.0f;
}
}  // namespace
}  // namespace la_sort
