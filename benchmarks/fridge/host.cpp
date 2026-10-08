#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <vector>

#include "workload.hpp"

namespace {
using namespace fridge::benchmark;

struct Measurement {
  double mean;
  double p99;
  double maximum;
  size_t overruns;
  double checksum;
};

std::optional<double> ParseSlowdown(int argc, char** argv);

Measurement MeasureScenario(size_t scenario, double budget) {
  using Clock = std::chrono::steady_clock;
  constexpr size_t blocks = fridge::kSampleRateHz;
  auto sound = std::make_unique<fridge::sound::Sound>();
  Workload workload(scenario);
  std::vector<double> times(blocks);
  double checksum = 0;
  size_t overruns = 0;
  double total = 0;
  for (size_t block = 0; block < blocks; ++block) {
    const auto start = Clock::now();
    for (size_t i = 0; i < kBlockSize; ++i) {
      const auto result = workload.Tick(*sound);
      checksum += result.left + result.right;
    }
    const double ns =
        std::chrono::duration<double, std::nano>(Clock::now() - start).count();
    times[block] = ns;
    total += ns;
    overruns += ns > budget;
  }
  std::sort(times.begin(), times.end());
  return {total / blocks, times[blocks * 99 / 100], times.back(), overruns,
          checksum};
}

// ----- Validation

std::optional<double> ParseSlowdown(int argc, char** argv) {
  if (argc == 1) {
    return 8.0;
  }
  if (argc != 2) {
    return std::nullopt;
  }
  char* end = nullptr;
  const double value = std::strtod(argv[1], &end);
  if (end == argv[1] || *end != '\0' || !std::isfinite(value) || value < 1) {
    return std::nullopt;
  }
  return value;
}
}  // namespace

int main(int argc, char** argv) {
  // A reduced budget is a host regression proxy, not Cortex-M7 emulation.
  const auto slowdown = ParseSlowdown(argc, argv);
  if (!slowdown) {
    std::cerr << "usage: fridge-benchmark [slowdown >= 1]\n";
    return 2;
  }
  const double budget = 1e9 * kBlockSize / fridge::kSampleRateHz / *slowdown;
  std::cout << "scenario,mean_ns,p99_ns,max_ns,budget_ns,overruns,checksum\n";
  bool passed = true;
  for (size_t scenario = 0; scenario < kScenarios.size(); ++scenario) {
    const auto result = MeasureScenario(scenario, budget);
    std::cout << kScenarios[scenario] << ',' << result.mean << ',' << result.p99
              << ',' << result.maximum << ',' << budget << ','
              << result.overruns << ',' << std::setprecision(10)
              << result.checksum << '\n';
    passed &= std::isfinite(result.checksum) && result.p99 <= budget;
  }
  return passed ? 0 : 1;
}
