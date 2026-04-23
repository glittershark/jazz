#ifndef VALUE_DISPLAY_H_
#define VALUE_DISPLAY_H_

#include <concepts>
#include <cstdint>
#include <type_traits>

#include "color.hpp"

namespace fridge::ui {

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

}  // namespace value_display

}  // namespace fridge::ui

#endif  // VALUE_DISPLAY_H_
