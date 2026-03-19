#ifdef UNIT_TEST

#include "granular.hpp"

#include <algorithm>
#include <array>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <sys/wait.h>
#include <vector>

namespace {

constexpr int kDefaultSampleRate = 48000;
constexpr int kDefaultChannels = 2;
constexpr size_t kFramesPerChunk = 1024;

struct Options {
  std::string input_path;
  std::optional<std::string> output_path = std::nullopt;
  bool play = true;
  int sample_rate = kDefaultSampleRate;
  int channels = kDefaultChannels;
  std::string effect_name = "granular";
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
};

enum class ParseKind { kOk, kHelp, kError };

struct ParseResult {
  ParseKind kind = ParseKind::kError;
  Options options;
};

const char *EffectListText() {
  return "bypass, granular, clip, anticlip, lowpass, highpass, "
         "cursed_lowpass, cursed_highpass, la_sort";
}

std::optional<EffectKind> ParseEffect(const std::string &name) {
  if (name == "bypass") {
    return EffectKind::kBypass;
  }
  if (name == "granular") {
    return EffectKind::kGranular;
  }
  if (name == "clip") {
    return EffectKind::kClip;
  }
  if (name == "anticlip") {
    return EffectKind::kAnticlip;
  }
  if (name == "lowpass") {
    return EffectKind::kLowPass;
  }
  if (name == "highpass") {
    return EffectKind::kHighPass;
  }
  if (name == "cursed_lowpass") {
    return EffectKind::kCursedLowPass;
  }
  if (name == "cursed_highpass") {
    return EffectKind::kCursedHighPass;
  }
  if (name == "la_sort") {
    return EffectKind::kLaSort;
  }
  return std::nullopt;
}

void PrintUsage(const char *argv0) {
  std::cerr << "Usage: " << argv0
            << " <input.mp3> [--output output.mp3] [--no-play]"
               " [--sample-rate hz] [--channels n]"
               " [--effect name] [--list-effects]\n";
  std::cerr << "Effects: " << EffectListText() << "\n";
}

bool ParseInt(const std::string &value, int *out) {
  char *end = nullptr;
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

ParseResult ParseArgs(int argc, char **argv) {
  ParseResult result;
  Options options;
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
      if (options.channels > 8) {
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
      options.effect_name = std::string(argv[++i]);
      if (!ParseEffect(options.effect_name).has_value()) {
        std::cerr << "invalid effect: " << options.effect_name << "\n";
        std::cerr << "available effects: " << EffectListText() << "\n";
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

  if (options.input_path.empty()) {
    PrintUsage(argv[0]);
    return result;
  }
  if (!options.play && !options.output_path.has_value()) {
    std::cerr << "nothing to do: enable playback or provide --output\n";
    return result;
  }

  result.kind = ParseKind::kOk;
  result.options = options;
  return result;
}

std::string ShellEscape(const std::string &input) {
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

bool CommandExists(const std::string &command) {
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

std::array<Head, NUM_HEADS> MakeDefaultHeads() {
  return {{
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
      },
  }};
}

class GranularProcessor {
public:
  GranularProcessor() : heads_(MakeDefaultHeads()) {}

  float Process(float sample) {
    for (auto &head : heads_) {
      head.Process(sample);
    }
    const float wet_signal = granular_.FullCycle(heads_);
    return std::clamp(sample + wet_signal, -1.0f, 1.0f);
  }

private:
  Granular granular_;
  std::array<Head, NUM_HEADS> heads_;
};

class SampleProcessor {
public:
  virtual ~SampleProcessor() = default;
  virtual float Process(float sample) = 0;
};

class BypassProcessor final : public SampleProcessor {
public:
  float Process(float sample) override { return sample; }
};

class GranularSampleProcessor final : public SampleProcessor {
public:
  float Process(float sample) override { return processor_.Process(sample); }

private:
  GranularProcessor processor_;
};

class ClipProcessor final : public SampleProcessor {
public:
  float Process(float sample) override {
    const float amplified = sample * 15.0f;
    const float clipped = std::clamp(amplified, -0.5f, 0.5f);
    const float residue = amplified - clipped;
    return amplified - residue;
  }
};

class AnticlipProcessor final : public SampleProcessor {
public:
  float Process(float sample) override {
    const float amplified = sample * 3.0f;
    const float clipped = std::clamp(amplified, -0.1f, 0.1f);
    const float residue = amplified - clipped;
    return amplified + residue;
  }
};

class LowPassProcessor final : public SampleProcessor {
public:
  float Process(float sample) override {
    last_output_ = sample * alpha_ + last_output_ * (1.0f - alpha_);
    return last_output_;
  }

private:
  const float alpha_ = 0.1f;
  float last_output_ = 0.0f;
};

class HighPassProcessor final : public SampleProcessor {
public:
  float Process(float sample) override {
    last_output_ = alpha_ * (last_output_ + sample - last_input_);
    last_input_ = sample;
    return last_output_;
  }

private:
  const float alpha_ = 0.1f;
  float last_output_ = 0.0f;
  float last_input_ = 0.0f;
};

class CursedLowPassProcessor final : public SampleProcessor {
public:
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
  const float alpha_ = 0.1f;
  float last_output_ = 1.0f;
};

class CursedHighPassProcessor final : public SampleProcessor {
public:
  float Process(float sample) override {
    const float sign =
        std::sin(sample) * std::sin(last_output_) * std::sin(last_input_);
    const float abs_sample = std::abs(sample);
    const float abs_last_output = std::abs(last_output_);
    const float abs_last_input = std::abs(last_input_);
    const float last_output_term = (abs_last_output == 0.0f)
                                       ? 0.0f
                                       : std::pow(abs_last_output, alpha_);
    const float sample_term =
        (abs_sample == 0.0f) ? 0.0f : std::pow(abs_sample, alpha_);
    const float last_input_term =
        (abs_last_input == 0.0f) ? 0.0f : std::pow(abs_last_input, 1.0f - alpha_);

    last_output_ = sign * last_output_term * sample_term * last_input_term;
    last_input_ = sample;
    return last_output_;
  }

private:
  const float alpha_ = 0.1f;
  float last_output_ = 1.0f;
  float last_input_ = 1.0f;
};

class LaSortProcessor final : public SampleProcessor {
public:
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
  const float weight_center_ = 0.5f;
  const float weight_sharpness_ = 2.0f;

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

std::unique_ptr<SampleProcessor> MakeProcessor(EffectKind effect) {
  switch (effect) {
  case EffectKind::kBypass:
    return std::make_unique<BypassProcessor>();
  case EffectKind::kGranular:
    return std::make_unique<GranularSampleProcessor>();
  case EffectKind::kClip:
    return std::make_unique<ClipProcessor>();
  case EffectKind::kAnticlip:
    return std::make_unique<AnticlipProcessor>();
  case EffectKind::kLowPass:
    return std::make_unique<LowPassProcessor>();
  case EffectKind::kHighPass:
    return std::make_unique<HighPassProcessor>();
  case EffectKind::kCursedLowPass:
    return std::make_unique<CursedLowPassProcessor>();
  case EffectKind::kCursedHighPass:
    return std::make_unique<CursedHighPassProcessor>();
  case EffectKind::kLaSort:
    return std::make_unique<LaSortProcessor>();
  }
  return std::make_unique<BypassProcessor>();
}

bool WriteAll(FILE *stream, const float *buffer, size_t samples) {
  const size_t bytes_total = samples * sizeof(float);
  const unsigned char *cursor =
      reinterpret_cast<const unsigned char *>(buffer);
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

int PcloseStatus(FILE *stream) {
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

} // namespace

int main(int argc, char **argv) {
  const ParseResult parsed = ParseArgs(argc, argv);
  if (parsed.kind == ParseKind::kHelp) {
    return 0;
  }
  if (parsed.kind != ParseKind::kOk) {
    return 2;
  }
  const Options options = parsed.options;
  std::signal(SIGPIPE, SIG_IGN);
  const auto effect = ParseEffect(options.effect_name);
  if (!effect.has_value()) {
    std::cerr << "invalid effect: " << options.effect_name << "\n";
    return 2;
  }

  if (!CommandExists("ffmpeg")) {
    std::cerr << "missing dependency: ffmpeg\n";
    return 1;
  }
  if (options.play && !CommandExists("ffplay")) {
    std::cerr << "missing dependency: ffplay (or use --no-play)\n";
    return 1;
  }
  const auto channel_layout = ChannelLayoutForCount(options.channels);
  if (!channel_layout.has_value()) {
    std::cerr << "unsupported channel count for playback: " << options.channels
              << "\n";
    return 1;
  }

  const std::string decoder_command =
      "ffmpeg -hide_banner -loglevel error -i " + ShellEscape(options.input_path) +
      " -f f32le -acodec pcm_f32le -ac " + std::to_string(options.channels) +
      " -ar " + std::to_string(options.sample_rate) + " pipe:1";

  FILE *decoder = popen(decoder_command.c_str(), "r");
  if (decoder == nullptr) {
    std::cerr << "failed to start ffmpeg decoder\n";
    return 1;
  }

  FILE *player = nullptr;
  if (options.play) {
    const std::string player_command =
        "ffplay -hide_banner -loglevel error -nodisp -autoexit -f f32le -ar " +
        std::to_string(options.sample_rate) + " -ch_layout " +
        *channel_layout + " -i -";
    player = popen(player_command.c_str(), "w");
    if (player == nullptr) {
      std::cerr << "failed to start ffplay\n";
      pclose(decoder);
      return 1;
    }
  }

  FILE *encoder = nullptr;
  if (options.output_path.has_value()) {
    const std::string encoder_command =
        "ffmpeg -hide_banner -loglevel error -y -f f32le -acodec pcm_f32le -ac " +
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

  std::vector<std::unique_ptr<SampleProcessor>> processors;
  processors.reserve(options.channels);
  for (int i = 0; i < options.channels; i++) {
    processors.push_back(MakeProcessor(*effect));
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
      chunk[i] = processors[channel]->Process(chunk[i]);
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

  if (decoder_status != 0) {
    std::cerr << "decoder failed with status " << decoder_status << "\n";
    return 1;
  }
  if (player != nullptr && player_status != 0) {
    std::cerr << "player failed with status " << player_status << "\n";
    return 1;
  }
  if (encoder != nullptr && encoder_status != 0) {
    std::cerr << "encoder failed with status " << encoder_status << "\n";
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
