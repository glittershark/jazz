#ifndef ENGINE_H_
#define ENGINE_H_

#include <cstdint>

#include "libjazz/units.hpp"

#ifndef UNIT_TEST

#include <array>
#include <chrono>

#include "config.hpp"
#include "config_transform.hpp"
#include "constants.hpp"
#include "head_transition.hpp"
#include "io.hpp"
#include "led.hpp"
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
  io::QuadratureEncoder position;
  io::QuadratureEncoder write_amount;
  io::QuadratureEncoder read_amount;
  io::QuadratureEncoder erase_amount;
  io::QuadratureEncoder feedback;
};

struct LfoKnobs {
  io::QuadratureEncoder range;
  io::QuadratureEncoder max_grain_size;
  io::QuadratureEncoder min_grain_size;
  io::QuadratureEncoder reverse_chance;
  io::QuadratureEncoder teleport_chance;
  io::QuadratureEncoder pitch_shift_chance;
  io::QuadratureEncoder low_octave_chance;
  io::QuadratureEncoder high_octave_chance;
};

class Engine {
  // hoookay: we have 13 knobs (2 channels each), 16 buttons (1 channel each),
  // and 8:1 muxes (in hardware), so we need 6 muxes in total to account for
  // everything.
  constexpr const static size_t kMuxes = 6;

  constexpr const static size_t kHeadKnobs = 5;
  constexpr const static size_t kLfoKnobs = 8;

  io::mux::MultiGpioInMux<kMuxes> mux_;
  io::mux::ChannelScan<8U, decltype(mux_)> scan_;

  Timer timer_;

  // see constructor for mux assignments
  io::QuadratureEncoder dry_;
  io::QuadratureEncoder wet_;
  HeadKnobs head_;
  LfoKnobs lfo_;
  std::array<io::Button, NUM_HEADS> head_select_;
  std::array<io::Button, NUM_LFOS> lfo_select_;

  // leds_ must be declared before ui_: C++ initializes members in declaration
  // order, and the UI constructor writes to the LED controller during init.
  io::led::Controller leds_;
  ui::UI ui_;
  transform::State transform_;
  transition::HeadTransitionMixer head_transitions_;

 public:
  Engine(config::Config initial_config = config::Config{});

  const transition::Frame& Tick(Samples<uint32_t> dt = Samples(1));
};

}  // namespace fridge::engine

#endif  // UNIT_TEST

#endif  // ENGINE_H_
