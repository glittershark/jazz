#ifdef UNIT_TEST

#include "granular.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
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
};

enum class ParseKind { kOk, kHelp, kError };

struct ParseResult {
  ParseKind kind = ParseKind::kError;
  Options options;
};

void PrintUsage(const char *argv0) {
  std::cerr << "Usage: " << argv0
            << " <input.mp3> [--output output.mp3] [--no-play]"
               " [--sample-rate hz] [--channels n]\n";
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

  if (!CommandExists("ffmpeg")) {
    std::cerr << "missing dependency: ffmpeg\n";
    return 1;
  }
  if (options.play && !CommandExists("ffplay")) {
    std::cerr << "missing dependency: ffplay (or use --no-play)\n";
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
        "ffplay -hide_banner -loglevel error -nodisp -autoexit -f f32le -ac " +
        std::to_string(options.channels) + " -ar " +
        std::to_string(options.sample_rate) + " -";
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

  std::vector<GranularProcessor> processors(options.channels);
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
      chunk[i] = processors[channel].Process(chunk[i]);
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
