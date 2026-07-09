#ifdef UNIT_TEST

#include <sys/wait.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include "config.hpp"
#include "config_transform.hpp"
#include "default_config.hpp"
#include "granular.hpp"
#include "head_transition.hpp"
#include "lfo_engine.hpp"
#include "sound.hpp"

namespace {

constexpr int kDefaultSampleRate = 48000;
constexpr int kDefaultChannels = 2;
constexpr size_t kFramesPerChunk = 1024;

fridge::config::Config DefaultFridgeConfig() {
  // Boot with exactly what the firmware boots with, so a host run reproduces
  // the hardware; presets and --fridge-* flags still override from there.
  return fridge::config::DefaultConfig();
}

struct EffectParams {
  float clip_threshold = 0.5f;
  float clip_gain = 15.0f;
  float anticlip_threshold = 0.1f;
  float anticlip_gain = 3.0f;
  float lowpass_alpha = 0.1f;
  float highpass_alpha = 0.1f;
  float cursed_lowpass_alpha = 0.1f;
  float cursed_highpass_alpha = 0.1f;
  float la_sort_weight_center = 0.5f;
  float la_sort_weight_sharpness = 2.0f;
  fridge::config::Config fridge_config = DefaultFridgeConfig();
};

struct Options {
  std::string input_path;
  std::optional<std::string> output_path = std::nullopt;
  bool play = true;
  int sample_rate = kDefaultSampleRate;
  int channels = kDefaultChannels;
  std::vector<std::string> effect_chain = {"granular"};
  EffectParams params;
  bool fridge_lfo_chart = false;
  bool fridge_lfo_chart_csv = false;
  size_t fridge_lfo_chart_index = 0;
  float fridge_lfo_chart_duration = 48000.0f;
  size_t fridge_lfo_chart_points = 1000;
  size_t fridge_lfo_chart_width = 72;
  size_t fridge_lfo_chart_height = 18;
};

enum class EffectKind {
  kBypass,
  kGranular,
  kClip,
  kAnticlip,
  kLowPass,
  kHighPass,
  kCursedLowPass,
  kCursedHighPass,
  kLaSort,
  kFridge,
};

enum class ParseKind { kOk, kHelp, kError };

struct ParseResult {
  ParseKind kind = ParseKind::kError;
  Options options;
};

const char* EffectListText() {
  return "bypass, granular, clip, anticlip, lowpass, highpass, "
         "cursed_lowpass, cursed_highpass, la_sort, fridge";
}

std::string ToLower(std::string value) {
  for (char& c : value) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return value;
}

std::string Trim(const std::string& value) {
  size_t start = 0;
  while (start < value.size() &&
         std::isspace(static_cast<unsigned char>(value[start]))) {
    start++;
  }

  size_t end = value.size();
  while (end > start &&
         std::isspace(static_cast<unsigned char>(value[end - 1]))) {
    end--;
  }

  return value.substr(start, end - start);
}

std::optional<std::string> Unquote(const std::string& value) {
  const std::string trimmed = Trim(value);
  if (trimmed.size() < 2) {
    return std::nullopt;
  }
  if (trimmed.front() != '"' || trimmed.back() != '"') {
    return std::nullopt;
  }
  return trimmed.substr(1, trimmed.size() - 2);
}

std::string CanonicalKey(std::string key) {
  key = ToLower(Trim(key));
  for (char& c : key) {
    if (c == '-') {
      c = '_';
    }
  }
  return key;
}

bool ParseInt(const std::string& value, int* out) {
  char* end = nullptr;
  long parsed = std::strtol(value.c_str(), &end, 10);
  if (*value.c_str() == '\0' || *end != '\0') {
    return false;
  }
  if (parsed <= 0 || parsed > std::numeric_limits<int>::max()) {
    return false;
  }
  *out = static_cast<int>(parsed);
  return true;
}

bool ParseFloat(const std::string& value, float* out) {
  char* end = nullptr;
  const float parsed = std::strtof(value.c_str(), &end);
  if (*value.c_str() == '\0' || *end != '\0') {
    return false;
  }
  if (!std::isfinite(parsed)) {
    return false;
  }
  *out = parsed;
  return true;
}

bool ParseSize(const std::string& value, size_t* out) {
  if (value.empty() || value[0] == '-') {
    return false;
  }

  char* end = nullptr;
  const unsigned long long parsed = std::strtoull(value.c_str(), &end, 10);
  if (*value.c_str() == '\0' || *end != '\0') {
    return false;
  }
  if (parsed > std::numeric_limits<size_t>::max()) {
    return false;
  }

  *out = static_cast<size_t>(parsed);
  return true;
}

std::optional<EffectKind> ParseEffect(const std::string& name) {
  const std::string effect = CanonicalKey(name);
  if (effect == "bypass") {
    return EffectKind::kBypass;
  }
  if (effect == "granular") {
    return EffectKind::kGranular;
  }
  if (effect == "clip") {
    return EffectKind::kClip;
  }
  if (effect == "anticlip") {
    return EffectKind::kAnticlip;
  }
  if (effect == "lowpass") {
    return EffectKind::kLowPass;
  }
  if (effect == "highpass") {
    return EffectKind::kHighPass;
  }
  if (effect == "cursed_lowpass") {
    return EffectKind::kCursedLowPass;
  }
  if (effect == "cursed_highpass") {
    return EffectKind::kCursedHighPass;
  }
  if (effect == "la_sort") {
    return EffectKind::kLaSort;
  }
  if (effect == "fridge") {
    return EffectKind::kFridge;
  }
  return std::nullopt;
}

std::vector<std::string> SplitCommaList(const std::string& value) {
  std::vector<std::string> items;
  std::stringstream ss(value);
  std::string part;

  while (std::getline(ss, part, ',')) {
    const std::string item = CanonicalKey(part);
    if (!item.empty()) {
      items.push_back(item);
    }
  }
  return items;
}

bool SetEffectChainFromSpec(const std::string& spec,
                            std::vector<std::string>* out, std::string* error) {
  const std::vector<std::string> names = SplitCommaList(spec);
  if (names.empty()) {
    *error = "effect chain is empty";
    return false;
  }

  for (const std::string& name : names) {
    if (!ParseEffect(name).has_value()) {
      *error = "invalid effect: " + name;
      return false;
    }
  }

  *out = names;
  return true;
}

bool ApplyParamValue(const std::string& key, float value, EffectParams* params,
                     std::string* error) {
  const std::string k = CanonicalKey(key);

  auto expect_range = [&](float lo, float hi, const char* label) {
    if (value < lo || value > hi) {
      *error = std::string(label) + " must be in [" + std::to_string(lo) +
               ", " + std::to_string(hi) + "]";
      return false;
    }
    return true;
  };

  if (k == "clip_threshold") {
    if (!expect_range(0.0f, 1.0f, "clip_threshold")) {
      return false;
    }
    params->clip_threshold = value;
    return true;
  }
  if (k == "clip_gain") {
    if (!expect_range(0.0f, 64.0f, "clip_gain")) {
      return false;
    }
    params->clip_gain = value;
    return true;
  }
  if (k == "anticlip_threshold") {
    if (!expect_range(0.0f, 1.0f, "anticlip_threshold")) {
      return false;
    }
    params->anticlip_threshold = value;
    return true;
  }
  if (k == "anticlip_gain") {
    if (!expect_range(0.0f, 64.0f, "anticlip_gain")) {
      return false;
    }
    params->anticlip_gain = value;
    return true;
  }
  if (k == "lowpass_alpha") {
    if (!expect_range(0.0f, 1.0f, "lowpass_alpha")) {
      return false;
    }
    params->lowpass_alpha = value;
    return true;
  }
  if (k == "highpass_alpha") {
    if (!expect_range(0.0f, 1.0f, "highpass_alpha")) {
      return false;
    }
    params->highpass_alpha = value;
    return true;
  }
  if (k == "cursed_lowpass_alpha") {
    if (!expect_range(0.0f, 1.0f, "cursed_lowpass_alpha")) {
      return false;
    }
    params->cursed_lowpass_alpha = value;
    return true;
  }
  if (k == "cursed_highpass_alpha") {
    if (!expect_range(0.0f, 1.0f, "cursed_highpass_alpha")) {
      return false;
    }
    params->cursed_highpass_alpha = value;
    return true;
  }
  if (k == "la_sort_weight_center") {
    if (!expect_range(0.0f, 1.0f, "la_sort_weight_center")) {
      return false;
    }
    params->la_sort_weight_center = value;
    return true;
  }
  if (k == "la_sort_weight_sharpness") {
    if (!expect_range(0.0f, 32.0f, "la_sort_weight_sharpness")) {
      return false;
    }
    params->la_sort_weight_sharpness = value;
    return true;
  }

  *error = "unknown preset/parameter key: " + key;
  return false;
}

bool ParseIndexedField(const std::string& value, size_t* index,
                       std::string* field) {
  const size_t separator = value.find('_');
  if (separator == std::string::npos) {
    return false;
  }

  if (!ParseSize(value.substr(0, separator), index)) {
    return false;
  }

  *field = value.substr(separator + 1);
  return !field->empty();
}

bool ParseFloatInRange(const std::string& value, float lo, float hi,
                       const std::string& label, float* out,
                       std::string* error) {
  float parsed = 0.0f;
  if (!ParseFloat(value, &parsed) || parsed < lo || parsed > hi) {
    *error = label + " must be in [" + std::to_string(lo) + ", " +
             std::to_string(hi) + "]";
    return false;
  }

  *out = parsed;
  return true;
}

bool ParseFridgeSize(const std::string& value, size_t minimum, size_t maximum,
                     const std::string& label, size_t* out,
                     std::string* error) {
  size_t parsed = 0;
  if (!ParseSize(value, &parsed) || parsed < minimum || parsed > maximum) {
    *error = label + " must be in [" + std::to_string(minimum) + ", " +
             std::to_string(maximum) + "]";
    return false;
  }

  *out = parsed;
  return true;
}

bool ApplyFridgeFeedbackKind(const std::string& value,
                             fridge::config::Feedback* feedback,
                             std::string* error) {
  const std::string kind = CanonicalKey(value);
  if (kind == "read") {
    feedback->kind = fridge::config::Feedback::Kind::kRead;
    return true;
  }
  if (kind == "erase") {
    feedback->kind = fridge::config::Feedback::Kind::kErase;
    return true;
  }

  *error = "fridge feedback kind must be read or erase";
  return false;
}

bool ApplyFridgeTargetObject(const std::string& value,
                             fridge::config::Target* target,
                             std::string* error) {
  const std::string object = CanonicalKey(value);
  if (object == "head") {
    target->object = fridge::config::TargetObject::kHead;
    return true;
  }
  if (object == "lfo") {
    target->object = fridge::config::TargetObject::kLFO;
    return true;
  }
  if (object == "mixer") {
    target->object = fridge::config::TargetObject::kMixer;
    return true;
  }

  *error = "fridge target object must be head, lfo, or mixer";
  return false;
}

std::optional<fridge::config::TargetParameter> ParseFridgeTargetParameter(
    const std::string& value) {
  const std::string parameter = CanonicalKey(value);

  if (parameter == "position") {
    return fridge::config::TargetParameter::kPosition;
  }
  if (parameter == "write_amount") {
    return fridge::config::TargetParameter::kWriteAmount;
  }
  if (parameter == "read_amount") {
    return fridge::config::TargetParameter::kReadAmount;
  }
  if (parameter == "erase_amount") {
    return fridge::config::TargetParameter::kEraseAmount;
  }
  if (parameter == "feedback_amount") {
    return fridge::config::TargetParameter::kFeedbackAmount;
  }
  if (parameter == "range") {
    return fridge::config::TargetParameter::kRange;
  }
  if (parameter == "max_grain_size") {
    return fridge::config::TargetParameter::kMaxGrainSize;
  }
  if (parameter == "min_grain_size") {
    return fridge::config::TargetParameter::kMinGrainSize;
  }
  if (parameter == "reverse_chance") {
    return fridge::config::TargetParameter::kReverseChance;
  }
  if (parameter == "teleport_chance") {
    return fridge::config::TargetParameter::kTeleportChance;
  }
  if (parameter == "pitch_shift_chance") {
    return fridge::config::TargetParameter::kPitchShiftChance;
  }
  if (parameter == "low_octave_chance") {
    return fridge::config::TargetParameter::kLowOctaveChance;
  }
  if (parameter == "high_octave_chance") {
    return fridge::config::TargetParameter::kHighOctaveChance;
  }
  if (parameter == "dry") {
    return fridge::config::TargetParameter::kDry;
  }
  if (parameter == "wet") {
    return fridge::config::TargetParameter::kWet;
  }

  return std::nullopt;
}

bool ApplyFridgeTargetParameter(const std::string& value,
                                fridge::config::Target* target,
                                std::string* error) {
  const auto parameter = ParseFridgeTargetParameter(value);
  if (!parameter.has_value()) {
    *error = "unknown fridge target parameter: " + value;
    return false;
  }

  target->parameter = *parameter;
  return true;
}

bool ApplyFridgeHeadValue(const std::string& field, const std::string& value,
                          fridge::config::Head* head, std::string* error) {
  if (field == "position") {
    return ParseFridgeSize(value, 0, fridge::BUFFER_LEN - 1,
                           "fridge head position", &head->position, error);
  }
  if (field == "write_amount") {
    return ParseFloatInRange(value, 0.0f, 1.0f, "fridge head write_amount",
                             &head->write_amount, error);
  }
  if (field == "read_amount") {
    return ParseFloatInRange(value, 0.0f, 1.0f, "fridge head read_amount",
                             &head->read_amount, error);
  }
  if (field == "erase_amount") {
    return ParseFloatInRange(value, 0.0f, 1.0f, "fridge head erase_amount",
                             &head->erase_amount, error);
  }
  if (field == "feedback_amount") {
    return ParseFloatInRange(value, 0.0f, 1.0f, "fridge head feedback_amount",
                             &head->feedback.amount, error);
  }
  if (field == "feedback_kind") {
    return ApplyFridgeFeedbackKind(value, &head->feedback, error);
  }

  *error = "unknown fridge head key: " + field;
  return false;
}

bool ApplyFridgeTargetValue(const std::string& field, const std::string& value,
                            fridge::config::Target* target,
                            std::string* error) {
  if (field == "object") {
    return ApplyFridgeTargetObject(value, target, error);
  }
  if (field == "parameter") {
    return ApplyFridgeTargetParameter(value, target, error);
  }
  if (field == "index" || field == "object_idx") {
    return ParseFridgeSize(value, 0, fridge::NUM_HEADS - 1,
                           "fridge target index", &target->object_idx, error);
  }

  *error = "unknown fridge target key: " + field;
  return false;
}

bool ApplyFridgeLfoValue(const std::string& field, const std::string& value,
                         fridge::config::LFO* lfo, std::string* error) {
  if (field == "range") {
    return ParseFridgeSize(value, 0, fridge::BUFFER_LEN - 1, "fridge lfo range",
                           &lfo->range, error);
  }
  if (field == "max_grain_size") {
    return ParseFridgeSize(value, 1, fridge::BUFFER_LEN,
                           "fridge lfo max_grain_size", &lfo->max_grain_size,
                           error);
  }
  if (field == "min_grain_size") {
    return ParseFridgeSize(value, 1, fridge::BUFFER_LEN,
                           "fridge lfo min_grain_size", &lfo->min_grain_size,
                           error);
  }
  if (field == "reverse_chance") {
    return ParseFloatInRange(value, 0.0f, 1.0f, "fridge lfo reverse_chance",
                             &lfo->reverse_chance, error);
  }
  if (field == "teleport_chance") {
    return ParseFloatInRange(value, 0.0f, 1.0f, "fridge lfo teleport_chance",
                             &lfo->teleport_chance, error);
  }
  if (field == "pitch_shift_chance") {
    return ParseFloatInRange(value, 0.0f, 1.0f, "fridge lfo pitch_shift_chance",
                             &lfo->pitch_shift_chance, error);
  }
  if (field == "low_octave_chance") {
    return ParseFloatInRange(value, 0.0f, 1.0f, "fridge lfo low_octave_chance",
                             &lfo->low_octave_chance, error);
  }
  if (field == "high_octave_chance") {
    return ParseFloatInRange(value, 0.0f, 1.0f, "fridge lfo high_octave_chance",
                             &lfo->high_octave_chance, error);
  }

  constexpr const char* target_prefix = "target_";
  if (field.starts_with(target_prefix)) {
    size_t target_idx = 0;
    std::string target_field;
    if (!ParseIndexedField(field.substr(std::strlen(target_prefix)),
                           &target_idx, &target_field) ||
        target_idx >= fridge::MAX_TARGET_PARAMS) {
      *error = "invalid fridge lfo target index";
      return false;
    }

    if (!lfo->targets[target_idx].has_value()) {
      lfo->targets[target_idx] = fridge::config::Target{};
    }
    return ApplyFridgeTargetValue(target_field, value,
                                  &lfo->targets[target_idx].value(), error);
  }

  *error = "unknown fridge lfo key: " + field;
  return false;
}

bool ApplyFridgeConfigKV(const std::string& key, const std::string& value,
                         fridge::config::Config* config, std::string* error) {
  const std::string k = CanonicalKey(key);

  if (k == "fridge_dry") {
    return ParseFloatInRange(value, 0.0f, 4.0f, "fridge dry", &config->dry,
                             error);
  }
  if (k == "fridge_wet") {
    return ParseFloatInRange(value, 0.0f, 4.0f, "fridge wet", &config->wet,
                             error);
  }

  constexpr const char* head_prefix = "fridge_head_";
  if (k.starts_with(head_prefix)) {
    size_t head_idx = 0;
    std::string field;
    if (!ParseIndexedField(k.substr(std::strlen(head_prefix)), &head_idx,
                           &field) ||
        head_idx >= fridge::NUM_HEADS) {
      *error = "invalid fridge head index";
      return false;
    }

    return ApplyFridgeHeadValue(field, value, &config->heads[head_idx], error);
  }

  constexpr const char* lfo_prefix = "fridge_lfo_";
  if (k.starts_with(lfo_prefix)) {
    size_t lfo_idx = 0;
    std::string field;
    if (!ParseIndexedField(k.substr(std::strlen(lfo_prefix)), &lfo_idx,
                           &field) ||
        lfo_idx >= fridge::NUM_LFOS) {
      *error = "invalid fridge lfo index";
      return false;
    }

    return ApplyFridgeLfoValue(field, value, &config->lfos[lfo_idx], error);
  }

  *error = "unknown fridge preset key: " + key;
  return false;
}

bool ApplyConfigKV(const std::string& key, const std::string& value,
                   Options* options, std::string* error) {
  const std::string k = CanonicalKey(key);

  if (k.starts_with("fridge_")) {
    return ApplyFridgeConfigKV(k, value, &options->params.fridge_config, error);
  }

  if (k == "effect" || k == "effects" || k == "effect_chain") {
    std::vector<std::string> chain;
    if (!SetEffectChainFromSpec(value, &chain, error)) {
      return false;
    }
    options->effect_chain = chain;
    return true;
  }

  if (k == "sample_rate") {
    int v = 0;
    if (!ParseInt(value, &v)) {
      *error = "invalid sample_rate in preset";
      return false;
    }
    options->sample_rate = v;
    return true;
  }

  if (k == "channels") {
    int v = 0;
    if (!ParseInt(value, &v) || v < 1 || v > 8) {
      *error = "invalid channels in preset (supported 1-8)";
      return false;
    }
    options->channels = v;
    return true;
  }

  float v = 0.0f;
  if (!ParseFloat(value, &v)) {
    *error = "invalid numeric value for key: " + key;
    return false;
  }
  return ApplyParamValue(k, v, &options->params, error);
}

void PlotLine(std::vector<std::string>& grid, int x0, int y0, int x1, int y1) {
  const int dx = std::abs(x1 - x0);
  const int sx = x0 < x1 ? 1 : -1;
  const int dy = -std::abs(y1 - y0);
  const int sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;

  while (true) {
    if (y0 >= 0 && y0 < static_cast<int>(grid.size()) && x0 >= 0 &&
        x0 < static_cast<int>(grid.front().size())) {
      grid[y0][x0] = '*';
    }

    if (x0 == x1 && y0 == y1) {
      break;
    }

    const int e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y0 += sy;
    }
  }
}

const char* FridgeTargetObjectName(fridge::config::TargetObject object) {
  switch (object) {
  case fridge::config::TargetObject::kHead:
    return "head";
  case fridge::config::TargetObject::kLFO:
    return "lfo";
  case fridge::config::TargetObject::kMixer:
    return "mixer";
  }

  return "unknown";
}

const char* FridgeTargetParameterName(fridge::config::TargetParameter param) {
  using Parameter = fridge::config::TargetParameter;

  switch (param) {
  case Parameter::kPosition:
    return "position";
  case Parameter::kWriteAmount:
    return "write_amount";
  case Parameter::kReadAmount:
    return "read_amount";
  case Parameter::kEraseAmount:
    return "erase_amount";
  case Parameter::kFeedbackAmount:
    return "feedback_amount";
  case Parameter::kRange:
    return "range";
  case Parameter::kMaxGrainSize:
    return "max_grain_size";
  case Parameter::kMinGrainSize:
    return "min_grain_size";
  case Parameter::kReverseChance:
    return "reverse_chance";
  case Parameter::kTeleportChance:
    return "teleport_chance";
  case Parameter::kPitchShiftChance:
    return "pitch_shift_chance";
  case Parameter::kLowOctaveChance:
    return "low_octave_chance";
  case Parameter::kHighOctaveChance:
    return "high_octave_chance";
  case Parameter::kDry:
    return "dry";
  case Parameter::kWet:
    return "wet";
  }

  return "unknown";
}

struct FridgeLfoTracePoint {
  float time;
  float value;
};

std::vector<FridgeLfoTracePoint> CollectFridgeLfoTrace(
    const fridge::config::LFO& lfo, float duration, size_t point_count) {
  point_count = std::max<size_t>(2, point_count);
  const float dt =
      point_count > 1 ? duration / static_cast<float>(point_count - 1) : 0.0f;

  fridge::state::LFOEngine engine(lfo, 1234);
  engine.Reset(0.0f, fridge::state::Direction::kForwards);

  std::vector<FridgeLfoTracePoint> points;
  points.reserve(point_count);
  points.push_back({.time = 0.0f, .value = engine.value()});

  for (size_t i = 1; i < point_count; ++i) {
    points.push_back({
        .time = dt * static_cast<float>(i),
        .value = engine.Tick(dt),
    });
  }

  return points;
}

bool GetFridgeLfoForChart(const Options& options,
                          const fridge::config::LFO** lfo) {
  const size_t lfo_idx = options.fridge_lfo_chart_index;
  if (lfo_idx >= fridge::NUM_LFOS) {
    std::cerr << "invalid --fridge-lfo-index: " << lfo_idx << "\n";
    return false;
  }

  *lfo = &options.params.fridge_config.lfos[lfo_idx];
  return true;
}

bool PrintFridgeLfoCsv(const Options& options) {
  const fridge::config::LFO* lfo = nullptr;
  if (!GetFridgeLfoForChart(options, &lfo)) {
    return false;
  }

  const float duration = std::max(0.0f, options.fridge_lfo_chart_duration);
  const std::vector<FridgeLfoTracePoint> points =
      CollectFridgeLfoTrace(*lfo, duration, options.fridge_lfo_chart_points);

  std::cout << "time,value\n";
  for (const FridgeLfoTracePoint& point : points) {
    std::cout << std::fixed << std::setprecision(6) << point.time << ","
              << point.value << "\n";
  }

  return true;
}

bool PrintFridgeLfoChart(const Options& options) {
  const fridge::config::LFO* lfo = nullptr;
  if (!GetFridgeLfoForChart(options, &lfo)) {
    return false;
  }

  const size_t width = std::max<size_t>(2, options.fridge_lfo_chart_width);
  const size_t height = std::max<size_t>(2, options.fridge_lfo_chart_height);
  const float duration = std::max(0.0f, options.fridge_lfo_chart_duration);
  const float max_value = std::max(1.0f, static_cast<float>(lfo->range));
  const std::vector<FridgeLfoTracePoint> points =
      CollectFridgeLfoTrace(*lfo, duration, width);

  std::vector<std::string> grid(height, std::string(width, ' '));

  auto y_for = [&](float value) {
    const float clamped = std::max(0.0f, std::min(value, max_value));
    const float normalized = clamped / max_value;
    return static_cast<int>((1.0f - normalized) * (height - 1));
  };

  for (size_t i = 1; i < points.size(); ++i) {
    PlotLine(grid, static_cast<int>(i - 1), y_for(points[i - 1].value),
             static_cast<int>(i), y_for(points[i].value));
  }

  std::cout << "fridge LFO " << options.fridge_lfo_chart_index
            << " range=" << lfo->range << " duration=" << duration << " samples"
            << " width=" << width << " seed=1234\n";
  std::cout << "targets:";
  bool has_target = false;
  for (const auto& target : lfo->targets) {
    if (target.has_value()) {
      std::cout << " " << FridgeTargetObjectName(target->object) << "."
                << target->object_idx << "."
                << FridgeTargetParameterName(target->parameter);
      has_target = true;
    }
  }
  if (!has_target) {
    std::cout << " none";
  }
  std::cout << "\n";

  for (size_t row = 0; row < height; ++row) {
    const float label =
        max_value * static_cast<float>(height - 1 - row) / (height - 1);
    std::cout << std::setw(8) << std::fixed << std::setprecision(1) << label
              << " |" << grid[row] << "\n";
  }
  std::cout << std::string(9, ' ') << "+" << std::string(width, '-') << "\n";
  std::cout << std::string(10, ' ') << "0"
            << std::string(width > 10 ? width - 10 : 1, ' ') << duration
            << "\n";
  return true;
}

bool ParseTomlPreset(const std::string& text, Options* options,
                     std::string* error) {
  std::istringstream in(text);
  std::string line;

  while (std::getline(in, line)) {
    const size_t comment = line.find('#');
    if (comment != std::string::npos) {
      line = line.substr(0, comment);
    }

    line = Trim(line);
    if (line.empty()) {
      continue;
    }

    const size_t eq = line.find('=');
    if (eq == std::string::npos) {
      *error = "invalid TOML line: " + line;
      return false;
    }

    const std::string key = Trim(line.substr(0, eq));
    const std::string raw_value = Trim(line.substr(eq + 1));

    if (CanonicalKey(key) == "effect_chain") {
      if (!raw_value.empty() && raw_value.front() == '[') {
        if (raw_value.back() != ']') {
          *error = "invalid effect_chain array in preset";
          return false;
        }
        const std::string inside = raw_value.substr(1, raw_value.size() - 2);
        std::vector<std::string> chain;
        std::stringstream ss(inside);
        std::string part;
        while (std::getline(ss, part, ',')) {
          const auto maybe_name = Unquote(part);
          if (!maybe_name.has_value()) {
            *error = "effect_chain array must contain quoted strings";
            return false;
          }
          chain.push_back(CanonicalKey(*maybe_name));
        }
        if (chain.empty()) {
          *error = "effect_chain array is empty";
          return false;
        }
        for (const auto& name : chain) {
          if (!ParseEffect(name).has_value()) {
            *error = "invalid effect in effect_chain: " + name;
            return false;
          }
        }
        options->effect_chain = chain;
        continue;
      }
    }

    std::string value = raw_value;
    if (const auto maybe_str = Unquote(raw_value); maybe_str.has_value()) {
      value = *maybe_str;
    }

    if (!ApplyConfigKV(key, value, options, error)) {
      return false;
    }
  }

  return true;
}

bool ParseJsonPreset(const std::string& text, Options* options,
                     std::string* error) {
  std::smatch match;
  std::regex chain_array("\"effect_chain\"\\s*:\\s*\\[(.*?)\\]",
                         std::regex::icase | std::regex::optimize);

  if (std::regex_search(text, match, chain_array)) {
    const std::string inside = match[1].str();
    std::vector<std::string> chain;
    std::regex quoted("\"([^\"]+)\"");
    auto begin = std::sregex_iterator(inside.begin(), inside.end(), quoted);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
      chain.push_back(CanonicalKey((*it)[1].str()));
    }

    if (chain.empty()) {
      *error = "effect_chain JSON array is empty";
      return false;
    }
    for (const auto& name : chain) {
      if (!ParseEffect(name).has_value()) {
        *error = "invalid effect in effect_chain: " + name;
        return false;
      }
    }
    options->effect_chain = chain;
  }

  std::regex string_pair("\"([A-Za-z0-9_-]+)\"\\s*:\\s*\"([^\"]*)\"",
                         std::regex::icase | std::regex::optimize);
  auto sbegin = std::sregex_iterator(text.begin(), text.end(), string_pair);
  auto send = std::sregex_iterator();
  for (auto it = sbegin; it != send; ++it) {
    const std::string key = (*it)[1].str();
    const std::string value = (*it)[2].str();
    const std::string ck = CanonicalKey(key);
    if (ck == "effect_chain") {
      continue;
    }
    if (!ApplyConfigKV(key, value, options, error)) {
      return false;
    }
  }

  std::regex number_pair(
      "\"([A-Za-z0-9_-]+)\"\\s*:\\s*(-?(?:\\d+\\.?\\d*|\\.\\d+)(?:[eE][+\\-]?"
      "\\d+)?)",
      std::regex::icase | std::regex::optimize);
  auto nbegin = std::sregex_iterator(text.begin(), text.end(), number_pair);
  auto nend = std::sregex_iterator();
  for (auto it = nbegin; it != nend; ++it) {
    const std::string key = (*it)[1].str();
    const std::string value = (*it)[2].str();
    if (!ApplyConfigKV(key, value, options, error)) {
      return false;
    }
  }

  std::regex effect_str("\"effect\"\\s*:\\s*\"([^\"]+)\"",
                        std::regex::icase | std::regex::optimize);
  if (std::regex_search(text, match, effect_str)) {
    std::vector<std::string> chain;
    if (!SetEffectChainFromSpec(match[1].str(), &chain, error)) {
      return false;
    }
    options->effect_chain = chain;
  }

  return true;
}

bool LoadPreset(const std::string& path, Options* options, std::string* error) {
  std::ifstream file(path);
  if (!file.good()) {
    *error = "failed to open preset file: " + path;
    return false;
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  const std::string text = buffer.str();

  std::string ext;
  const size_t dot = path.find_last_of('.');
  if (dot != std::string::npos) {
    ext = ToLower(path.substr(dot + 1));
  }

  if (ext == "json") {
    return ParseJsonPreset(text, options, error);
  }
  if (ext == "toml") {
    return ParseTomlPreset(text, options, error);
  }

  const std::string trimmed = Trim(text);
  if (!trimmed.empty() && trimmed.front() == '{') {
    return ParseJsonPreset(text, options, error);
  }
  return ParseTomlPreset(text, options, error);
}

void PrintUsage(const char* argv0) {
  std::cerr << "Usage: " << argv0
            << " <input.mp3> [--output output.mp3] [--no-play]"
               " [--sample-rate hz] [--channels n]"
               " [--effect e1,e2,...] [--preset preset.{json|toml}]"
               " [--fridge-lfo-chart] [--list-effects]\n";
  std::cerr << "Effects: " << EffectListText() << "\n";
  std::cerr
      << "Params:\n"
      << "  --clip-threshold [0..1] --clip-gain [0..64]\n"
      << "  --anticlip-threshold [0..1] --anticlip-gain [0..64]\n"
      << "  --lowpass-alpha [0..1] --highpass-alpha [0..1]\n"
      << "  --cursed-lowpass-alpha [0..1] --cursed-highpass-alpha [0..1]\n"
      << "  --la-sort-weight-center [0..1]"
      << " --la-sort-weight-sharpness [0..32]\n"
      << "  --fridge-lfo-chart"
      << " --fridge-lfo-chart-csv"
      << " --fridge-lfo-index N"
      << " --fridge-lfo-chart-duration samples"
      << " --fridge-lfo-chart-points N"
      << " --fridge-lfo-chart-width N"
      << " --fridge-lfo-chart-height N\n"
      << "  fridge preset keys: fridge_dry, fridge_wet,"
      << " fridge_head_N_*, fridge_lfo_N_*\n";
}

ParseResult ParseArgs(int argc, char** argv) {
  ParseResult result;
  Options options;

  std::optional<std::string> preset_path = std::nullopt;
  for (int i = 1; i < argc; i++) {
    const std::string arg(argv[i]);
    if (arg == "--preset") {
      if (i + 1 >= argc) {
        std::cerr << "missing value for --preset\n";
        return result;
      }
      preset_path = std::string(argv[++i]);
    }
  }

  if (preset_path.has_value()) {
    std::string error;
    if (!LoadPreset(*preset_path, &options, &error)) {
      std::cerr << error << "\n";
      return result;
    }
  }

  auto parse_next_float = [&](int* idx, float* out, const char* name) {
    if (*idx + 1 >= argc || !ParseFloat(argv[++*idx], out)) {
      std::cerr << "invalid " << name << "\n";
      return false;
    }
    return true;
  };

  auto parse_next_size = [&](int* idx, size_t* out, const char* name) {
    if (*idx + 1 >= argc || !ParseSize(argv[++*idx], out)) {
      std::cerr << "invalid " << name << "\n";
      return false;
    }
    return true;
  };

  auto apply_param = [&](const char* key, float value) {
    std::string error;
    if (!ApplyParamValue(key, value, &options.params, &error)) {
      std::cerr << error << "\n";
      return false;
    }
    return true;
  };

  for (int i = 1; i < argc; i++) {
    std::string arg(argv[i]);

    if (arg == "--help" || arg == "-h") {
      PrintUsage(argv[0]);
      result.kind = ParseKind::kHelp;
      return result;
    }
    if (arg == "--list-effects") {
      std::cout << EffectListText() << "\n";
      result.kind = ParseKind::kHelp;
      return result;
    }
    if (arg == "--preset") {
      i++;
      continue;
    }
    if (arg == "--output") {
      if (i + 1 >= argc) {
        std::cerr << "missing value for --output\n";
        return result;
      }
      options.output_path = std::string(argv[++i]);
      continue;
    }
    if (arg == "--no-play") {
      options.play = false;
      continue;
    }
    if (arg == "--sample-rate") {
      if (i + 1 >= argc || !ParseInt(argv[++i], &options.sample_rate)) {
        std::cerr << "invalid --sample-rate\n";
        return result;
      }
      continue;
    }
    if (arg == "--channels") {
      if (i + 1 >= argc || !ParseInt(argv[++i], &options.channels)) {
        std::cerr << "invalid --channels\n";
        return result;
      }
      if (options.channels < 1 || options.channels > 8) {
        std::cerr << "supported channel count: 1-8\n";
        return result;
      }
      continue;
    }
    if (arg == "--effect") {
      if (i + 1 >= argc) {
        std::cerr << "missing value for --effect\n";
        return result;
      }
      std::string error;
      if (!SetEffectChainFromSpec(argv[++i], &options.effect_chain, &error)) {
        std::cerr << error << "\n";
        std::cerr << "available effects: " << EffectListText() << "\n";
        return result;
      }
      continue;
    }
    if (arg == "--fridge-lfo-chart") {
      options.fridge_lfo_chart = true;
      continue;
    }
    if (arg == "--fridge-lfo-chart-csv") {
      options.fridge_lfo_chart = true;
      options.fridge_lfo_chart_csv = true;
      continue;
    }
    if (arg == "--fridge-lfo-index") {
      if (!parse_next_size(&i, &options.fridge_lfo_chart_index,
                           "--fridge-lfo-index")) {
        return result;
      }
      continue;
    }
    if (arg == "--fridge-lfo-chart-duration") {
      if (!parse_next_float(&i, &options.fridge_lfo_chart_duration,
                            "--fridge-lfo-chart-duration")) {
        return result;
      }
      continue;
    }
    if (arg == "--fridge-lfo-chart-points") {
      if (!parse_next_size(&i, &options.fridge_lfo_chart_points,
                           "--fridge-lfo-chart-points")) {
        return result;
      }
      continue;
    }
    if (arg == "--fridge-lfo-chart-width") {
      if (!parse_next_size(&i, &options.fridge_lfo_chart_width,
                           "--fridge-lfo-chart-width")) {
        return result;
      }
      continue;
    }
    if (arg == "--fridge-lfo-chart-height") {
      if (!parse_next_size(&i, &options.fridge_lfo_chart_height,
                           "--fridge-lfo-chart-height")) {
        return result;
      }
      continue;
    }

    if (arg == "--clip-threshold") {
      float v = 0.0f;
      if (!parse_next_float(&i, &v, "--clip-threshold") ||
          !apply_param("clip_threshold", v)) {
        return result;
      }
      continue;
    }
    if (arg == "--clip-gain") {
      float v = 0.0f;
      if (!parse_next_float(&i, &v, "--clip-gain") ||
          !apply_param("clip_gain", v)) {
        return result;
      }
      continue;
    }
    if (arg == "--anticlip-threshold") {
      float v = 0.0f;
      if (!parse_next_float(&i, &v, "--anticlip-threshold") ||
          !apply_param("anticlip_threshold", v)) {
        return result;
      }
      continue;
    }
    if (arg == "--anticlip-gain") {
      float v = 0.0f;
      if (!parse_next_float(&i, &v, "--anticlip-gain") ||
          !apply_param("anticlip_gain", v)) {
        return result;
      }
      continue;
    }
    if (arg == "--lowpass-alpha") {
      float v = 0.0f;
      if (!parse_next_float(&i, &v, "--lowpass-alpha") ||
          !apply_param("lowpass_alpha", v)) {
        return result;
      }
      continue;
    }
    if (arg == "--highpass-alpha") {
      float v = 0.0f;
      if (!parse_next_float(&i, &v, "--highpass-alpha") ||
          !apply_param("highpass_alpha", v)) {
        return result;
      }
      continue;
    }
    if (arg == "--cursed-lowpass-alpha") {
      float v = 0.0f;
      if (!parse_next_float(&i, &v, "--cursed-lowpass-alpha") ||
          !apply_param("cursed_lowpass_alpha", v)) {
        return result;
      }
      continue;
    }
    if (arg == "--cursed-highpass-alpha") {
      float v = 0.0f;
      if (!parse_next_float(&i, &v, "--cursed-highpass-alpha") ||
          !apply_param("cursed_highpass_alpha", v)) {
        return result;
      }
      continue;
    }
    if (arg == "--la-sort-weight-center") {
      float v = 0.0f;
      if (!parse_next_float(&i, &v, "--la-sort-weight-center") ||
          !apply_param("la_sort_weight_center", v)) {
        return result;
      }
      continue;
    }
    if (arg == "--la-sort-weight-sharpness") {
      float v = 0.0f;
      if (!parse_next_float(&i, &v, "--la-sort-weight-sharpness") ||
          !apply_param("la_sort_weight_sharpness", v)) {
        return result;
      }
      continue;
    }

    if (!arg.empty() && arg[0] == '-') {
      std::cerr << "unknown option: " << arg << "\n";
      return result;
    }

    if (!options.input_path.empty()) {
      std::cerr << "multiple input files provided\n";
      return result;
    }
    options.input_path = arg;
  }

  if (options.input_path.empty() && !options.fridge_lfo_chart) {
    PrintUsage(argv[0]);
    return result;
  }
  if (!options.play && !options.output_path.has_value() &&
      !options.fridge_lfo_chart) {
    std::cerr << "nothing to do: enable playback or provide --output\n";
    return result;
  }

  result.kind = ParseKind::kOk;
  result.options = options;
  return result;
}

std::string ShellEscape(const std::string& input) {
  std::string escaped = "'";
  for (char c : input) {
    if (c == '\'') {
      escaped += "'\\''";
    } else {
      escaped.push_back(c);
    }
  }
  escaped.push_back('\'');
  return escaped;
}

bool CommandExists(const std::string& command) {
  std::string check = "command -v " + command + " >/dev/null 2>&1";
  return std::system(check.c_str()) == 0;
}

std::optional<std::string> ChannelLayoutForCount(int channels) {
  switch (channels) {
  case 1:
    return "mono";
  case 2:
    return "stereo";
  case 3:
    return "2.1";
  case 4:
    return "quad";
  case 5:
    return "5.0";
  case 6:
    return "5.1";
  case 7:
    return "6.1";
  case 8:
    return "7.1";
  default:
    return std::nullopt;
  }
}

std::array<granular::Head, granular::NUM_HEADS> MakeDefaultHeads() {
  return {{
      {
          .kind = granular::Head::Kind::kWrite,
          .direction = granular::Head::Direction::kBackwards,
          .index = 0,
          .value = 1.0f,
          .step = 1,
      },
      {
          .kind = granular::Head::Kind::kRead,
          .direction = granular::Head::Direction::kForwards,
          .index = 10000,
          .value = 0.7f,
          .step = 1,
          .random = {{.grain_size = 10000}},
      },
      {
          .kind = granular::Head::Kind::kRead,
          .direction = granular::Head::Direction::kBackwards,
          .index = 20000,
          .value = 0.7f,
          .step = 2,
          .random = {{.grain_size = 20000}},
      },
      {
          .kind = granular::Head::Kind::kErase,
          .direction = granular::Head::Direction::kBackwards,
          .index = granular::BUFFER_LEN / 2,
          .value = 0.5f,
      },
  }};
}

class SampleProcessor {
 public:
  virtual ~SampleProcessor() = default;
  virtual float Process(float sample) = 0;
};

class BypassProcessor final : public SampleProcessor {
 public:
  float Process(float sample) override { return sample; }
};

class GranularProcessor final : public SampleProcessor {
 public:
  GranularProcessor() : heads_(MakeDefaultHeads()) {}

  float Process(float sample) override {
    for (auto& head : heads_) {
      head.Process(sample);
    }
    const float wet_signal = granular_.FullCycle(heads_);
    return std::clamp(sample + wet_signal, -1.0f, 1.0f);
  }

 private:
  granular::Granular granular_;
  std::array<granular::Head, granular::NUM_HEADS> heads_;
};

class FridgeProcessor final : public SampleProcessor {
 public:
  explicit FridgeProcessor(const fridge::config::Config& config)
      : root_config_(config) {
    transform_.Reset(root_config_);
  }

  float Process(float sample) override {
    const fridge::config::Config& config =
        transform_.Update(root_config_, 1.0f);
    const fridge::transition::Frame& frame =
        head_transitions_.Update(config, transform_.head_transitions());
    return sound_.ProcessSample(frame, sample);
  }

 private:
  fridge::config::Config root_config_;
  fridge::transform::state transform_;
  fridge::transition::HeadTransitionMixer head_transitions_;
  fridge::sound::Sound sound_;
};

class ClipProcessor final : public SampleProcessor {
 public:
  ClipProcessor(float threshold, float gain)
      : threshold_(threshold), gain_(gain) {}

  float Process(float sample) override {
    const float amplified = sample * gain_;
    const float clipped = std::clamp(amplified, -threshold_, threshold_);
    const float residue = amplified - clipped;
    return amplified - residue;
  }

 private:
  float threshold_;
  float gain_;
};

class AnticlipProcessor final : public SampleProcessor {
 public:
  AnticlipProcessor(float threshold, float gain)
      : threshold_(threshold), gain_(gain) {}

  float Process(float sample) override {
    const float amplified = sample * gain_;
    const float clipped = std::clamp(amplified, -threshold_, threshold_);
    const float residue = amplified - clipped;
    return amplified + residue;
  }

 private:
  float threshold_;
  float gain_;
};

class LowPassProcessor final : public SampleProcessor {
 public:
  explicit LowPassProcessor(float alpha) : alpha_(alpha) {}

  float Process(float sample) override {
    last_output_ = sample * alpha_ + last_output_ * (1.0f - alpha_);
    return last_output_;
  }

 private:
  float alpha_;
  float last_output_ = 0.0f;
};

class HighPassProcessor final : public SampleProcessor {
 public:
  explicit HighPassProcessor(float alpha) : alpha_(alpha) {}

  float Process(float sample) override {
    last_output_ = alpha_ * (last_output_ + sample - last_input_);
    last_input_ = sample;
    return last_output_;
  }

 private:
  float alpha_;
  float last_output_ = 0.0f;
  float last_input_ = 0.0f;
};

class CursedLowPassProcessor final : public SampleProcessor {
 public:
  explicit CursedLowPassProcessor(float alpha) : alpha_(alpha) {}

  float Process(float sample) override {
    const float sign = std::sin(sample) * std::sin(last_output_);
    const float abs_sample = std::abs(sample);
    const float abs_last = std::abs(last_output_);

    const float sample_term =
        (abs_sample == 0.0f) ? 0.0f : std::pow(abs_sample, alpha_);
    const float last_term =
        (abs_last == 0.0f) ? 0.0f : std::pow(abs_last, 1.0f - alpha_);

    last_output_ = sign * sample_term * last_term;
    return last_output_;
  }

 private:
  float alpha_;
  float last_output_ = 1.0f;
};

class CursedHighPassProcessor final : public SampleProcessor {
 public:
  explicit CursedHighPassProcessor(float alpha) : alpha_(alpha) {}

  float Process(float sample) override {
    const float sign =
        std::sin(sample) * std::sin(last_output_) * std::sin(last_input_);

    const float abs_sample = std::abs(sample);
    const float abs_last_output = std::abs(last_output_);
    const float abs_last_input = std::abs(last_input_);

    const float last_output_term =
        (abs_last_output == 0.0f) ? 0.0f : std::pow(abs_last_output, alpha_);
    const float sample_term =
        (abs_sample == 0.0f) ? 0.0f : std::pow(abs_sample, alpha_);
    const float last_input_term = (abs_last_input == 0.0f)
                                      ? 0.0f
                                      : std::pow(abs_last_input, 1.0f - alpha_);

    last_output_ = sign * last_output_term * sample_term * last_input_term;
    last_input_ = sample;
    return last_output_;
  }

 private:
  float alpha_;
  float last_output_ = 1.0f;
  float last_input_ = 1.0f;
};

class LaSortProcessor final : public SampleProcessor {
 public:
  LaSortProcessor(float weight_center, float weight_sharpness)
      : weight_center_(weight_center), weight_sharpness_(weight_sharpness) {}

  float Process(float sample) override {
    InsertSorted(sample);
    return WeightedTap();
  }

 private:
  static constexpr size_t kBufferSize = 64;
  std::array<float, kBufferSize> insertion_buffer_{};
  std::array<float, kBufferSize> sorted_samples_{};
  size_t current_size_ = 0;
  size_t write_index_ = 0;
  float weight_center_;
  float weight_sharpness_;

  void RemoveFromSorted(float value) {
    size_t remove_pos = 0;
    for (size_t i = 0; i < current_size_; i++) {
      if (sorted_samples_[i] == value) {
        remove_pos = i;
        break;
      }
    }
    for (size_t i = remove_pos; i < current_size_ - 1; i++) {
      sorted_samples_[i] = sorted_samples_[i + 1];
    }
    current_size_--;
  }

  void InsertSorted(float sample) {
    if (current_size_ < kBufferSize) {
      size_t insert_pos = current_size_;
      for (size_t i = 0; i < current_size_; i++) {
        if (sample < sorted_samples_[i]) {
          insert_pos = i;
          break;
        }
      }

      for (size_t i = current_size_; i > insert_pos; i--) {
        sorted_samples_[i] = sorted_samples_[i - 1];
      }
      sorted_samples_[insert_pos] = sample;
      current_size_++;
      insertion_buffer_[write_index_] = sample;
      write_index_ = (write_index_ + 1) % kBufferSize;
      return;
    }

    RemoveFromSorted(insertion_buffer_[write_index_]);
    size_t insert_pos = current_size_;
    for (size_t i = 0; i < current_size_; i++) {
      if (sample < sorted_samples_[i]) {
        insert_pos = i;
        break;
      }
    }

    for (size_t i = current_size_; i > insert_pos; i--) {
      sorted_samples_[i] = sorted_samples_[i - 1];
    }
    sorted_samples_[insert_pos] = sample;
    current_size_++;
    insertion_buffer_[write_index_] = sample;
    write_index_ = (write_index_ + 1) % kBufferSize;
  }

  float WeightedTap() const {
    if (current_size_ == 0) {
      return 0.0f;
    }
    if (current_size_ == 1) {
      return sorted_samples_[0];
    }

    float sum = 0.0f;
    float weight_sum = 0.0f;

    for (size_t i = 0; i < current_size_; i++) {
      const float normalized_pos =
          static_cast<float>(i) / static_cast<float>(current_size_ - 1);
      const float distance = std::abs(normalized_pos - weight_center_);
      const float weight = std::exp(-weight_sharpness_ * distance);

      sum += sorted_samples_[i] * weight;
      weight_sum += weight;
    }

    return (weight_sum > 0.0f) ? (sum / weight_sum) : 0.0f;
  }
};

std::unique_ptr<SampleProcessor> MakeProcessor(EffectKind effect,
                                               const EffectParams& params) {
  switch (effect) {
  case EffectKind::kBypass:
    return std::make_unique<BypassProcessor>();
  case EffectKind::kGranular:
    return std::make_unique<GranularProcessor>();
  case EffectKind::kClip:
    return std::make_unique<ClipProcessor>(params.clip_threshold,
                                           params.clip_gain);
  case EffectKind::kAnticlip:
    return std::make_unique<AnticlipProcessor>(params.anticlip_threshold,
                                               params.anticlip_gain);
  case EffectKind::kLowPass:
    return std::make_unique<LowPassProcessor>(params.lowpass_alpha);
  case EffectKind::kHighPass:
    return std::make_unique<HighPassProcessor>(params.highpass_alpha);
  case EffectKind::kCursedLowPass:
    return std::make_unique<CursedLowPassProcessor>(
        params.cursed_lowpass_alpha);
  case EffectKind::kCursedHighPass:
    return std::make_unique<CursedHighPassProcessor>(
        params.cursed_highpass_alpha);
  case EffectKind::kLaSort:
    return std::make_unique<LaSortProcessor>(params.la_sort_weight_center,
                                             params.la_sort_weight_sharpness);
  case EffectKind::kFridge:
    return std::make_unique<FridgeProcessor>(params.fridge_config);
  }

  return std::make_unique<BypassProcessor>();
}

bool WriteAll(FILE* stream, const float* buffer, size_t samples) {
  const size_t bytes_total = samples * sizeof(float);
  const auto cursor = reinterpret_cast<const unsigned char*>(buffer);
  size_t bytes_written = 0;

  while (bytes_written < bytes_total) {
    const size_t n = std::fwrite(cursor + bytes_written, 1,
                                 bytes_total - bytes_written, stream);
    if (n == 0) {
      return false;
    }
    bytes_written += n;
  }

  return true;
}

int PcloseStatus(FILE* stream) {
  if (stream == nullptr) {
    return 0;
  }
  const int status = pclose(stream);
  if (status == -1) {
    return 1;
  }
  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }
  return 1;
}

}  // namespace

int main(int argc, char** argv) {
  const ParseResult parsed = ParseArgs(argc, argv);
  if (parsed.kind == ParseKind::kHelp) {
    return 0;
  }
  if (parsed.kind != ParseKind::kOk) {
    return 2;
  }

  const Options options = parsed.options;
  std::signal(SIGPIPE, SIG_IGN);

  if (options.fridge_lfo_chart) {
    bool chart_ok = options.fridge_lfo_chart_csv ? PrintFridgeLfoCsv(options)
                                                 : PrintFridgeLfoChart(options);
    if (!chart_ok) {
      return 2;
    }
    if (options.input_path.empty()) {
      return 0;
    }
  }

  std::vector<EffectKind> effect_chain;
  effect_chain.reserve(options.effect_chain.size());
  for (const auto& name : options.effect_chain) {
    const auto kind = ParseEffect(name);
    if (!kind.has_value()) {
      std::cerr << "invalid effect in chain: " << name << "\n";
      return 2;
    }
    effect_chain.push_back(*kind);
  }

  if (!CommandExists("ffmpeg")) {
    std::cerr << "missing dependency: ffmpeg\n";
    return 1;
  }
  if (options.play && !CommandExists("ffplay")) {
    std::cerr << "missing dependency: ffplay (or use --no-play)\n";
    return 1;
  }

  std::optional<std::string> channel_layout = std::nullopt;
  if (options.play) {
    channel_layout = ChannelLayoutForCount(options.channels);
    if (!channel_layout.has_value()) {
      std::cerr << "unsupported channel count for playback: "
                << options.channels << "\n";
      return 1;
    }
  }

  const std::string decoder_command =
      "ffmpeg -hide_banner -loglevel error -i " +
      ShellEscape(options.input_path) + " -f f32le -acodec pcm_f32le -ac " +
      std::to_string(options.channels) + " -ar " +
      std::to_string(options.sample_rate) + " pipe:1";

  FILE* decoder = popen(decoder_command.c_str(), "r");
  if (decoder == nullptr) {
    std::cerr << "failed to start ffmpeg decoder\n";
    return 1;
  }

  FILE* player = nullptr;
  if (options.play) {
    const std::string player_command =
        "ffplay -hide_banner -loglevel error -nodisp -autoexit -f f32le -ar " +
        std::to_string(options.sample_rate) + " -ch_layout " + *channel_layout +
        " -i -";
    player = popen(player_command.c_str(), "w");
    if (player == nullptr) {
      std::cerr << "failed to start ffplay\n";
      pclose(decoder);
      return 1;
    }
  }

  FILE* encoder = nullptr;
  if (options.output_path.has_value()) {
    const std::string encoder_command =
        "ffmpeg -hide_banner -loglevel error -y -f f32le -acodec pcm_f32le "
        "-ac " +
        std::to_string(options.channels) + " -ar " +
        std::to_string(options.sample_rate) +
        " -i pipe:0 -vn -codec:a libmp3lame -q:a 2 " +
        ShellEscape(*options.output_path);
    encoder = popen(encoder_command.c_str(), "w");
    if (encoder == nullptr) {
      std::cerr << "failed to start ffmpeg encoder\n";
      if (player != nullptr) {
        pclose(player);
      }
      pclose(decoder);
      return 1;
    }
  }

  std::vector<std::vector<std::unique_ptr<SampleProcessor>>> chains;
  chains.resize(options.channels);
  for (int channel = 0; channel < options.channels; channel++) {
    auto& chain = chains[channel];
    chain.reserve(effect_chain.size());
    for (EffectKind effect : effect_chain) {
      chain.push_back(MakeProcessor(effect, options.params));
    }
  }

  std::vector<float> chunk(options.channels * kFramesPerChunk);

  bool write_failed = false;
  while (true) {
    const size_t read_samples =
        std::fread(chunk.data(), sizeof(float), chunk.size(), decoder);
    if (read_samples == 0) {
      break;
    }

    for (size_t i = 0; i < read_samples; i++) {
      const int channel = i % options.channels;
      float sample = chunk[i];
      for (auto& processor : chains[channel]) {
        sample = processor->Process(sample);
      }
      chunk[i] = std::clamp(sample, -1.0f, 1.0f);
    }

    if (player != nullptr && !WriteAll(player, chunk.data(), read_samples)) {
      std::cerr << "failed writing to ffplay\n";
      write_failed = true;
      break;
    }
    if (encoder != nullptr && !WriteAll(encoder, chunk.data(), read_samples)) {
      std::cerr << "failed writing to encoder\n";
      write_failed = true;
      break;
    }
  }

  if (std::ferror(decoder)) {
    std::cerr << "error reading decoded samples from ffmpeg\n";
    write_failed = true;
  }

  const int decoder_status = PcloseStatus(decoder);
  const int player_status = PcloseStatus(player);
  const int encoder_status = PcloseStatus(encoder);

  if (player != nullptr && player_status != 0) {
    std::cerr << "player failed with status " << player_status << "\n";
    return 1;
  }
  if (encoder != nullptr && encoder_status != 0) {
    std::cerr << "encoder failed with status " << encoder_status << "\n";
    return 1;
  }
  if (decoder_status != 0) {
    std::cerr << "decoder failed with status " << decoder_status << "\n";
    return 1;
  }
  if (write_failed) {
    return 1;
  }

  return 0;
}

#else

#include "daisy_seed.h"

using namespace daisy;

static DaisySeed hw;

int main(void) {
  hw.Configure();
  hw.Init();
  hw.StartLog();

  while (1) {
    System::Delay(500);
    hw.PrintLine("test thing: %s", "string");
  }
}

#endif
