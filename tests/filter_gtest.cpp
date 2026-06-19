#include <gtest/gtest.h>

#include <cmath>

#include "libjazz/filter.hpp"

using namespace jazz::filter;

static InfiniteImpulse<4> demo_filter() {
  /*
    IIR elliptic filter generated with the following octave code:

    octave:34> rp
    rp = 1
    octave:35> rs
    rs = 40
    octave:36> wp
    wp = 0.062500
    octave:37> ws
    ws = 0.1042
    octave:38> N = ellipord(wp, ws, rp, rs)
    N = 4
    octave:39> [b, a] = ellip(N, rp, rs, wp, "low")
    b =

       0.010368  -0.035994   0.051689  -0.035994   0.010368

    a =

       1.0000  -3.7756   5.3889  -3.4445   0.8316

  */
  return InfiniteImpulse<4>(
      {
          1.036797740838754e-02,
          -3.599350201178753e-02,
          5.168856017752318e-02,
          -3.599350201178754e-02,
          1.036797740838754e-02,
      },
      {
          -3.775615470141257,
          5.388911442196438,
          -3.444450284468981,
          0.831645207796911,
      });
}

TEST(InfiniteImpulseTest, roughly_the_same_as_in_octave) {
  std::array input = {
      0.f, 1.f, 0.f, 1.f, 0.f, 1.f, 0.f, 1.f,
  };

  const std::array expected_output = {
      0.f,       0.010368f, 0.003152f, 0.018085f,
      0.015022f, 0.033916f, 0.034788f, 0.057701f,
  };

  auto filter = demo_filter();

  filter.Filter(input.begin(), input.end());

  for (std::size_t i = 0; i < input.size(); ++i) {
    EXPECT_FLOAT_EQ(std::round(input[i] * 1e6) / 1e6, expected_output[i]);
  }
}
