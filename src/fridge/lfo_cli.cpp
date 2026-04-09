#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "fridge.hpp"

namespace {

using fridge::config::LFO;
using fridge::state::Direction;
using fridge::state::LFOEngine;

struct RunConfig {
  enum class OutputMode { kGraph, kCsv };

  LFO lfo;
  float duration = 32.0f;
  float dt = 1.0f;
  uint32_t seed = 1234;
  float initial_value = 0.0f;
  Direction direction = Direction::kForwards;
  OutputMode output_mode = OutputMode::kGraph;
  bool include_header = true;
  size_t width = 72;
  size_t height = 18;
};

struct SamplePoint {
  float time;
  float value;
  float speed;
  Direction direction;
  size_t grain_size;
  float grain_time_remaining;
};

std::string Trim(const std::string& input) {
  size_t start = input.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) {
    return "";
  }

  size_t end = input.find_last_not_of(" \t\r\n");
  return input.substr(start, end - start + 1);
}

float ParseFloat(const std::string& name, const std::string& value) {
  try {
    size_t idx = 0;
    float parsed = std::stof(value, &idx);
    if (idx != value.size()) {
      throw std::invalid_argument("trailing characters");
    }
    return parsed;
  } catch (const std::exception&) {
    throw std::runtime_error("invalid float for " + name + ": " + value);
  }
}

size_t ParseSize(const std::string& name, const std::string& value) {
  try {
    size_t idx = 0;
    unsigned long parsed = std::stoul(value, &idx);
    if (idx != value.size()) {
      throw std::invalid_argument("trailing characters");
    }
    return static_cast<size_t>(parsed);
  } catch (const std::exception&) {
    throw std::runtime_error("invalid integer for " + name + ": " + value);
  }
}

uint32_t ParseSeed(const std::string& name, const std::string& value) {
  return static_cast<uint32_t>(ParseSize(name, value));
}

Direction ParseDirection(const std::string& value) {
  if (value == "forward") {
    return Direction::kForwards;
  }
  if (value == "backward") {
    return Direction::kBackwards;
  }
  throw std::runtime_error("direction must be forward or backward");
}

void ApplySetting(RunConfig& config, const std::string& key,
                  const std::string& value) {
  if (key == "range") {
    config.lfo.range = ParseSize(key, value);
  } else if (key == "max_grain_size") {
    config.lfo.max_grain_size = ParseSize(key, value);
  } else if (key == "min_grain_size") {
    config.lfo.min_grain_size = ParseSize(key, value);
  } else if (key == "reverse_chance") {
    config.lfo.reverse_chance = ParseFloat(key, value);
  } else if (key == "teleport_chance") {
    config.lfo.teleport_chance = ParseFloat(key, value);
  } else if (key == "pitch_shift_chance") {
    config.lfo.pitch_shift_chance = ParseFloat(key, value);
  } else if (key == "low_octave_chance") {
    config.lfo.low_octave_chance = ParseFloat(key, value);
  } else if (key == "high_octave_chance") {
    config.lfo.high_octave_chance = ParseFloat(key, value);
  } else if (key == "duration") {
    config.duration = ParseFloat(key, value);
  } else if (key == "dt") {
    config.dt = ParseFloat(key, value);
  } else if (key == "seed") {
    config.seed = ParseSeed(key, value);
  } else if (key == "initial_value") {
    config.initial_value = ParseFloat(key, value);
  } else if (key == "direction") {
    config.direction = ParseDirection(value);
  } else {
    throw std::runtime_error("unknown setting: " + key);
  }
}

void LoadConfigFile(RunConfig& config, const std::string& path) {
  std::ifstream input(path);
  if (!input) {
    throw std::runtime_error("unable to open config: " + path);
  }

  std::string line;
  size_t lineno = 0;
  while (std::getline(input, line)) {
    ++lineno;
    std::string trimmed = Trim(line);
    if (trimmed.empty() || trimmed[0] == '#') {
      continue;
    }

    size_t eq = trimmed.find('=');
    if (eq == std::string::npos) {
      throw std::runtime_error("expected key = value at line " +
                               std::to_string(lineno));
    }

    std::string key = Trim(trimmed.substr(0, eq));
    std::string value = Trim(trimmed.substr(eq + 1));
    ApplySetting(config, key, value);
  }
}

void PrintUsage(std::ostream& out) {
  out << "Usage: fridge-lfo-cli [--config FILE] [options]\n"
      << "Render an ASCII graph of the fridge LFO over time.\n\n"
      << "Options:\n"
      << "  --config FILE\n"
      << "  --duration FLOAT\n"
      << "  --dt FLOAT\n"
      << "  --seed INT\n"
      << "  --initial-value FLOAT\n"
      << "  --direction forward|backward\n"
      << "  --range INT\n"
      << "  --max-grain-size INT\n"
      << "  --min-grain-size INT\n"
      << "  --reverse-chance FLOAT\n"
      << "  --teleport-chance FLOAT\n"
      << "  --pitch-shift-chance FLOAT\n"
      << "  --low-octave-chance FLOAT\n"
      << "  --high-octave-chance FLOAT\n"
      << "  --width INT\n"
      << "  --height INT\n"
      << "  --csv\n"
      << "  --graph\n"
      << "  --no-header\n"
      << "  --list-examples\n"
      << "  --help\n";
}

void PrintExamples(std::ostream& out) {
  out << "configs/fridge/slow_scan.cfg\n"
      << "configs/fridge/reverse_bounce.cfg\n"
      << "configs/fridge/teleport_sparks.cfg\n"
      << "configs/fridge/pitch_chaos.cfg\n";
}

void PrintPoint(float time, const LFOEngine& engine) {
  std::cout << std::fixed << std::setprecision(6) << time << ','
            << engine.value() << ',' << engine.speed() << ','
            << (engine.direction() == Direction::kForwards ? "forward"
                                                           : "backward")
            << ',' << engine.grain_size() << ','
            << engine.grain_time_remaining() << '\n';
}

std::vector<SamplePoint> CollectTrace(const RunConfig& config) {
  LFOEngine engine(config.lfo, config.seed);
  engine.Reset(config.initial_value, config.direction);

  std::vector<SamplePoint> samples;
  samples.push_back({.time = 0.0f,
                     .value = engine.value(),
                     .speed = engine.speed(),
                     .direction = engine.direction(),
                     .grain_size = engine.grain_size(),
                     .grain_time_remaining = engine.grain_time_remaining()});

  for (float time = config.dt; time <= config.duration + 1e-6f;
       time += config.dt) {
    engine.Tick(config.dt);
    samples.push_back({.time = time,
                       .value = engine.value(),
                       .speed = engine.speed(),
                       .direction = engine.direction(),
                       .grain_size = engine.grain_size(),
                       .grain_time_remaining = engine.grain_time_remaining()});
  }

  return samples;
}

void PlotLine(std::vector<std::string>& grid, int x0, int y0, int x1, int y1) {
  int dx = std::abs(x1 - x0);
  int sx = x0 < x1 ? 1 : -1;
  int dy = -std::abs(y1 - y0);
  int sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;

  while (true) {
    if (y0 >= 0 && y0 < static_cast<int>(grid.size()) && x0 >= 0 &&
        x0 < static_cast<int>(grid.front().size())) {
      grid[y0][x0] = '*';
    }

    if (x0 == x1 && y0 == y1) {
      break;
    }

    int e2 = 2 * err;
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

void PrintGraph(const RunConfig& config,
                const std::vector<SamplePoint>& samples) {
  size_t width = std::max<size_t>(2, config.width);
  size_t height = std::max<size_t>(2, config.height);
  float max_value = std::max(1.0f, static_cast<float>(config.lfo.range));

  std::vector<std::string> grid(height, std::string(width, ' '));

  auto x_for = [&](size_t index) {
    if (samples.size() <= 1 || width <= 1) {
      return 0;
    }
    return static_cast<int>((index * (width - 1)) / (samples.size() - 1));
  };
  auto y_for = [&](float value) {
    float clamped = std::max(0.0f, std::min(value, max_value));
    float normalized = clamped / max_value;
    return static_cast<int>((1.0f - normalized) * (height - 1));
  };

  for (size_t i = 1; i < samples.size(); ++i) {
    PlotLine(grid, x_for(i - 1), y_for(samples[i - 1].value), x_for(i),
             y_for(samples[i].value));
  }

  std::cout << "range=" << config.lfo.range << " duration=" << config.duration
            << " dt=" << config.dt << " seed=" << config.seed << '\n';
  std::cout << "speed uses 1.0, 0.5, or 2.0 depending on pitch-shift settings"
            << '\n';

  for (size_t row = 0; row < height; ++row) {
    float label =
        max_value * static_cast<float>(height - 1 - row) / (height - 1);
    std::cout << std::setw(7) << std::fixed << std::setprecision(2) << label
              << " |" << grid[row] << '\n';
  }

  std::cout << std::string(8, ' ') << '+' << std::string(width, '-') << '\n';
  std::ostringstream start;
  std::ostringstream end;
  start << std::fixed << std::setprecision(2) << 0.0f;
  end << std::fixed << std::setprecision(2) << config.duration;
  size_t padding = width > end.str().size() ? width - end.str().size()
                                            : static_cast<size_t>(0);
  std::cout << std::string(9, ' ') << start.str()
            << std::string(padding > start.str().size()
                               ? padding - start.str().size()
                               : 1,
                           ' ')
            << end.str() << '\n';
}

}  // namespace

int main(int argc, char** argv) {
  try {
    RunConfig config;

    for (int i = 1; i < argc; ++i) {
      std::string arg = argv[i];
      if (arg == "--help") {
        PrintUsage(std::cout);
        return 0;
      }
      if (arg == "--list-examples") {
        PrintExamples(std::cout);
        return 0;
      }
      if (arg == "--no-header") {
        config.include_header = false;
        continue;
      }
      if (arg == "--csv") {
        config.output_mode = RunConfig::OutputMode::kCsv;
        continue;
      }
      if (arg == "--graph") {
        config.output_mode = RunConfig::OutputMode::kGraph;
        continue;
      }

      auto require_value = [&](const char* flag) -> std::string {
        if (i + 1 >= argc) {
          throw std::runtime_error(std::string("missing value for ") + flag);
        }
        return argv[++i];
      };

      if (arg == "--config") {
        LoadConfigFile(config, require_value("--config"));
      } else if (arg == "--duration") {
        config.duration = ParseFloat("duration", require_value("--duration"));
      } else if (arg == "--dt") {
        config.dt = ParseFloat("dt", require_value("--dt"));
      } else if (arg == "--seed") {
        config.seed = ParseSeed("seed", require_value("--seed"));
      } else if (arg == "--initial-value") {
        config.initial_value =
            ParseFloat("initial_value", require_value("--initial-value"));
      } else if (arg == "--direction") {
        config.direction = ParseDirection(require_value("--direction"));
      } else if (arg == "--range") {
        config.lfo.range = ParseSize("range", require_value("--range"));
      } else if (arg == "--max-grain-size") {
        config.lfo.max_grain_size =
            ParseSize("max_grain_size", require_value("--max-grain-size"));
      } else if (arg == "--min-grain-size") {
        config.lfo.min_grain_size =
            ParseSize("min_grain_size", require_value("--min-grain-size"));
      } else if (arg == "--reverse-chance") {
        config.lfo.reverse_chance =
            ParseFloat("reverse_chance", require_value("--reverse-chance"));
      } else if (arg == "--teleport-chance") {
        config.lfo.teleport_chance =
            ParseFloat("teleport_chance", require_value("--teleport-chance"));
      } else if (arg == "--pitch-shift-chance") {
        config.lfo.pitch_shift_chance = ParseFloat(
            "pitch_shift_chance", require_value("--pitch-shift-chance"));
      } else if (arg == "--low-octave-chance") {
        config.lfo.low_octave_chance = ParseFloat(
            "low_octave_chance", require_value("--low-octave-chance"));
      } else if (arg == "--high-octave-chance") {
        config.lfo.high_octave_chance = ParseFloat(
            "high_octave_chance", require_value("--high-octave-chance"));
      } else if (arg == "--width") {
        config.width = ParseSize("width", require_value("--width"));
      } else if (arg == "--height") {
        config.height = ParseSize("height", require_value("--height"));
      } else {
        throw std::runtime_error("unknown argument: " + arg);
      }
    }

    if (config.dt <= 0.0f) {
      throw std::runtime_error("dt must be > 0");
    }
    if (config.duration < 0.0f) {
      throw std::runtime_error("duration must be >= 0");
    }

    std::vector<SamplePoint> samples = CollectTrace(config);
    if (config.output_mode == RunConfig::OutputMode::kCsv) {
      if (config.include_header) {
        std::cout
            << "time,value,speed,direction,grain_size,grain_time_remaining\n";
      }
      for (const auto& sample : samples) {
        std::cout << std::fixed << std::setprecision(6) << sample.time << ','
                  << sample.value << ',' << sample.speed << ','
                  << (sample.direction == Direction::kForwards ? "forward"
                                                               : "backward")
                  << ',' << sample.grain_size << ','
                  << sample.grain_time_remaining << '\n';
      }
    } else {
      PrintGraph(config, samples);
    }

    return 0;
  } catch (const std::exception& err) {
    std::cerr << "fridge-lfo-cli: " << err.what() << '\n';
    PrintUsage(std::cerr);
    return 1;
  }
}
