#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "la_sort.hpp"

#ifndef UNIT_TEST
#include <atomic>

#include "../fridge/io.hpp"
#include "../fridge/led.hpp"
#endif

namespace la_sort {

struct KnobBinding {
  float Config::* parameter;
  float maximum;
  uint8_t led_column;
  float minimum = 0.0f;
};

// First Fridge encoder bank, indexed by mux channel. The null binding is the
// integer length control; all other bindings address float config fields.
inline constexpr size_t kLengthChannel = 4;
inline constexpr std::array<KnobBinding, 7> kKnobs{{
    {&Config::wet, 1.0f, 1},                           // SW3: Wet
    {&Config::dry, 1.0f, 0},                           // SW2: Dry
    {&Config::weight_center, 1.0f, 2},                 // SW4: Position
    {&Config::weight_sharpness, kMaxSharpness, 3},     // SW5: Write Amount
    {nullptr, float(kMaxLength), 4, 1.0f},             // SW6: Read Amount
    {&Config::lfo_rate, kMaxLfoRate, 5, kMinLfoRate},  // SW7: Erase Amount
    {&Config::lfo_depth, 1.0f, 6},                     // SW8: Feedback
}};
inline constexpr int kTicksPerTurn = 96;

// Continuous controls span their range in one turn (rate logarithmically);
// length steps by one sample
// per quadrature tick. Returns true only when
// the value changed; turning against an end stop does not rebuild weights.
bool ApplyKnobTicks(Config& config, size_t channel, int ticks);

#ifndef UNIT_TEST
class Panel {
 public:
  explicit Panel(const Config& initial);

  // Consume queued encoder ticks and refresh LEDs from the main loop.
  bool Read(Config& config);

 private:
  void Display(size_t channel, const Config& config);

  fridge::io::mux::MultiGpioInMux<2> mux_;
  fridge::io::mux::ChannelScan<8, decltype(mux_)> scan_;
  daisy::TimerHandle timer_;
  std::array<fridge::io::QuadratureEncoder, kKnobs.size()> encoders_;
  std::array<std::atomic<int>, kKnobs.size()> pending_ticks_{};
  fridge::io::led::Controller leds_;
};
#endif

}  // namespace la_sort
