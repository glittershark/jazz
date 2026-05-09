#ifndef VALUE_DISPLAY_H_
#define VALUE_DISPLAY_H_

#include <sys/types.h>

#include <array>
#include <cassert>
#include <concepts>
#include <cstdint>
#include <iostream>
#include <type_traits>

#include "libjazz/color.hpp"

namespace fridge::ui {

using namespace jazz;

/*
 * Various methods of displaying some "current value" using a single RGB LED
 *
 * Some experimentation is in order here; we have a hunch that rotating through
 * the hue wheel from blue to cyan might look nice, but it might also be
 * completely illegible.
 */

/* A ValueDisplay is anything that can be called with a value (here arg, a
 * uint8_t) and returns a color */
template <typename Fn>
concept ValueDisplay =
    std::regular_invocable<Fn, uint8_t> &&
    std::convertible_to<std::invoke_result_t<Fn, uint8_t>, color::RGB>;

namespace value_display {

/* Display a value by rotating around the hue wheel, with per-color gamma
 * correction
 */
struct HueWheel {
  float red_gamma = 1.;
  float green_gamma = 1.;
  float blue_gamma = 1.;

  uint8_t start = 0;
  uint8_t end = 255;

  uint8_t saturation = 255;
  uint8_t value = 255;

  color::RGB operator()(uint8_t value) const;
};
static_assert(ValueDisplay<HueWheel>);
static_assert(std::semiregular<HueWheel>);

/* Display a value as a linear interpolation between two points in XYZ color
 * space */
struct CieInterp {
  color::XYZ start;
  color::XYZ end;

  color::XYZ operator()(uint8_t value) const;
};
static_assert(ValueDisplay<CieInterp>);

template <std::size_t N>
struct MultiSegmentCieInterp {
  struct Point {
    color::XYZ color;
    uint8_t point;
  };

  color::XYZ start;
  std::array<Point, N> midpoints;
  color::XYZ end;

  color::XYZ operator()(uint8_t value) const {
    for (ssize_t i = -1; i < static_cast<ssize_t>(N); i++) {
      auto interval_start =
          i == -1 ? Point{.color = start, .point = 0} : midpoints[i];
      auto interval_end =
          i == N - 1 ? Point{.color = end, .point = 255} : midpoints[i + 1];
      if (value == interval_start.point) {
        return interval_start.color;
      } else if (value == interval_end.point) {
        return interval_end.color;
      } else if (value >= interval_start.point && value <= interval_end.point) {
        auto scaled = static_cast<uint8_t>(
            static_cast<float>(value) -
            static_cast<float>(interval_start.point) /
                (static_cast<float>(interval_end.point) -
                 static_cast<float>(interval_start.point)) *
                255.f);
        return (CieInterp{.start = interval_start.color,
                          .end = interval_end.color})(scaled);
      }
    }
    return end;
  }
};
static_assert(ValueDisplay<MultiSegmentCieInterp<0>>);
static_assert(ValueDisplay<MultiSegmentCieInterp<1>>);
static_assert(ValueDisplay<MultiSegmentCieInterp<2>>);

}  // namespace value_display

}  // namespace fridge::ui

#endif  // VALUE_DISPLAY_H_
