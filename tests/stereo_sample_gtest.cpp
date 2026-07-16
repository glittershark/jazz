#include <gtest/gtest.h>
#include <rapidcheck/gtest.h>

#include "gtest/gtest.h"
#include "libjazz/gen.hpp"
#include "libjazz/stereo_sample.hpp"

using namespace jazz::audio;

TEST(StereoSampleTest, Mono) {
  StereoSample sample{
      .left = 0.7,
      .right = 0.1,
  };
  auto mono = sample.Mono();
  ASSERT_FLOAT_EQ(mono, 0.4);
}

RC_GTEST_PROP(StereoSampleTest, MonoOfMonoRoundTrip, ()) {
  const auto sample = *jazz::gen::ratio();
  auto stereo = StereoSample::OfMono(sample);
  auto round_tripped = stereo.Mono();

  ASSERT_FLOAT_EQ(round_tripped, sample);
}

TEST(StereoSampleTest, Panned) {
  auto sample = StereoSample::OfMono(0.7);
  auto panned = sample.Panned(Pan::Left(0.5));

  // Pan::Left(0.5) => p=-0.5; left stays at unity, right fades to 0.5
  ASSERT_FLOAT_EQ(panned.left, 0.7f);
  ASSERT_FLOAT_EQ(panned.right, 0.35f);
}

TEST(StereoSampleTest, PannedCenterIsUnity) {
  auto sample = StereoSample::OfMono(0.7);
  auto panned = sample.Panned(Pan::Center());

  ASSERT_FLOAT_EQ(panned.left, 0.7f);
  ASSERT_FLOAT_EQ(panned.right, 0.7f);
}
