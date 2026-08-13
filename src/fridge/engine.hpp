#ifndef ENGINE_H_
#define ENGINE_H_

#include <cstdint>

#include "libjazz/units.hpp"

#ifndef UNIT_TEST

#include <array>
#include <chrono>

#include "config.hpp"
#include "constants.hpp"
#include "io.hpp"
#include "led.hpp"
#include "mod.hpp"
#include "ui.hpp"

namespace fridge::engine {

using namespace std::chrono_literals;
using jazz::units::Samples;

/**
 * Calls a given callback at some microsecond period, consuming a timer
 * peripheral to do so. Used for stuff that, you know, needs regular, even
 * updates.
 */
class Timer {
  Callback<> callback_;
  daisy::TimerHandle timer_;
  std::chrono::microseconds period_;

  static void timer_callback_(void* this_) {
    static_cast<Timer*>(this_)->tick_();
  }

  void tick_() { callback_(); }

 public:
  Timer(const Timer&) = delete;
  Timer& operator=(const Timer&) = delete;

  Timer(daisy::TimerHandle::Config::Peripheral timer, Callback<> callback,
        std::chrono::microseconds period = 100us);

  daisy::TimerHandle::Result Start() { return timer_.Start(); }
};

struct HeadKnobs {
  io::PushbuttonQuadratureEncoder position;
  io::PushbuttonQuadratureEncoder write_amount;
  io::PushbuttonQuadratureEncoder read_amount;
  io::PushbuttonQuadratureEncoder erase_amount;
  io::PushbuttonQuadratureEncoder feedback;
  io::PushbuttonQuadratureEncoder pan;
};

struct LfoKnobs {
  io::PushbuttonQuadratureEncoder range;
  io::PushbuttonQuadratureEncoder max_grain_size;
  io::PushbuttonQuadratureEncoder min_grain_size;
  io::PushbuttonQuadratureEncoder reverse_chance;
  io::PushbuttonQuadratureEncoder teleport_chance;
  io::PushbuttonQuadratureEncoder pitch_shift_chance;
  io::PushbuttonQuadratureEncoder low_octave_chance;
  io::PushbuttonQuadratureEncoder high_octave_chance;
};

class Engine {
 public:
  constexpr const static size_t kMuxes = 8;
  constexpr const static size_t kHeadKnobs = 5;
  constexpr const static size_t kLfoKnobs = 8;

 private:
  io::mux::MultiGpioInMux<kMuxes> mux_;
  io::mux::ChannelScan<8U, decltype(mux_)> scan_;

  Timer timer_;

  // see constructor for mux assignments
  io::PushbuttonQuadratureEncoder dry_;
  io::PushbuttonQuadratureEncoder wet_;
  HeadKnobs head_;
  LfoKnobs lfo_;
  std::array<io::Button, kNumHeads> head_select_;
  std::array<io::Button, kNumLfos> lfo_select_;

  // leds_ must be declared before ui_: C++ initializes members in declaration
  // order, and the UI constructor writes to the LED controller during init.
  io::led::Controller leds_;
  ui::UI ui_;
  mod::Modulator modulator_;
  config::Config installed_config_;

 public:
  Engine(config::Config initial_config = config::Config{});

  ui::UI& ui() { return ui_; }
  const ui::UI& ui() const { return ui_; }

  /** Install a new root config (cheap enough for occasional knob changes,
   * not for the audio path). */
  void SetConfig(const config::Config& config);

  /** Pull the UI's config and install it if a knob changed it. Call from the
   * main loop, never the audio path. */
  void SyncConfig();

  /** Advance the modulation world one sample; feed the result to Sound. */
  const mod::Frame& TickSample() { return modulator_.TickSample(); }
};

}  // namespace fridge::engine

#endif  // UNIT_TEST

#endif  // ENGINE_H_
