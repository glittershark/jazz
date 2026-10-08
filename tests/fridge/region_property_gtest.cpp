#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <random>
#include <vector>

#include "gtest/gtest.h"
#include "regions.hpp"

using namespace fridge;

class RegionPropertyTest : public testing::TestWithParam<uint32_t> {};

TEST_P(RegionPropertyTest, RandomResizesPreserveExactlyTheRetainedPrefix) {
  std::mt19937 rng(GetParam());
  auto memory = std::make_unique<regions::Memory>();
  std::array<config::Region, kNumRegions> config;
  std::array<std::vector<std::optional<size_t>>, kNumRegions> observed;
  for (size_t i = 0; i < kNumRegions; ++i) {
    config[i].range = 1024;
    observed[i].resize(config[i].range);
  }
  ASSERT_TRUE(memory->SetRegions(config));
  for (size_t edit = 0; edit < 500; ++edit) {
    const size_t changed = rng() % kNumRegions;
    config[changed].range = 1 + rng() % 8192;
    observed[changed].resize(config[changed].range);
    ASSERT_TRUE(memory->SetRegions(config));
    for (size_t read = 0; read < 32; ++read) {
      const size_t region = rng() % kNumRegions;
      const size_t position = rng() % config[region].range;
      const auto address = memory->Resolve(region, position);
      const auto& prior = observed[region][position];
      ASSERT_EQ(address.fresh, !prior.has_value()) << "edit=" << edit;
      ASSERT_LT(address.index, kRegionCapacity);
      if (prior) {
        EXPECT_EQ(address.index, *prior);
      }
      observed[region][position] = address.index;
      const auto wrapped =
          memory->Resolve(region, position + config[region].range);
      EXPECT_EQ(wrapped.index, address.index);
      EXPECT_FALSE(wrapped.fresh);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(Seeds, RegionPropertyTest,
                         testing::Values(0u, 1u, 2u, 17u, 1234u, 98765u,
                                         0x80000000u, UINT32_MAX));

TEST(RegionBoundaryTest, FullPoolRedistributionIsIndependentOfRegionOrder) {
  auto memory = std::make_unique<regions::Memory>();
  std::array<config::Region, kNumRegions> config;
  for (auto& region : config) {
    region.range = kRegionPageSize;
  }
  config.back().range = kRegionCapacity - (kNumRegions - 1) * kRegionPageSize;
  ASSERT_TRUE(memory->SetRegions(config));
  const auto retained = memory->Resolve(3, 10);
  std::swap(config.front().range, config.back().range);
  ASSERT_TRUE(memory->SetRegions(config));
  EXPECT_EQ(memory->Resolve(3, 10).index, retained.index);
  EXPECT_FALSE(memory->Resolve(3, 10).fresh);
  EXPECT_TRUE(memory->Resolve(0, config.front().range - 1).fresh);
}

TEST(RegionBoundaryTest,
     InvalidFirstAllocationDoesNotPoisonLaterInitialization) {
  auto memory = std::make_unique<regions::Memory>();
  std::array<config::Region, kNumRegions> config;
  config[0].range = 0;
  EXPECT_FALSE(memory->SetRegions(config));
  config[0].range = 1;
  ASSERT_TRUE(memory->SetRegions(config));
  EXPECT_TRUE(memory->Resolve(0, SIZE_MAX).fresh);
  EXPECT_FALSE(memory->Resolve(0, 0).fresh);
}

TEST(RegionBoundaryTest, WrappingMatchesRoundedCircularReference) {
  for (size_t range : {1u, 2u, 3u, 1023u, 1024u, 65537u}) {
    for (float position : {-100000.5f, -10.5f, -1.5f, -0.5f, 0.0f, 0.5f, 1.5f,
                           1023.5f, 100000.5f}) {
      double wrapped = std::fmod(static_cast<double>(position), range);
      if (wrapped < 0) {
        wrapped += range;
      }
      const size_t expected =
          static_cast<size_t>(std::floor(wrapped + 0.5)) % range;
      EXPECT_EQ(regions::WrapPosition(position, range), expected)
          << "range=" << range << " position=" << position;
    }
  }
  for (float value : {NAN, INFINITY, -INFINITY}) {
    EXPECT_EQ(regions::WrapPosition(value, 100), 0);
  }
  EXPECT_EQ(regions::WrapPosition(10, 0), 0);
}
