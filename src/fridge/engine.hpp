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

/**
 * The thing that makes the world go 'round!
 */
class BreadboardEngine {
  io::mux::MultiGpioInMux<2> encoders_;
  io::mux::ChannelScan<8U, decltype(encoders_)> scan_;
  Timer timer_;

  io::QuadratureEncoder enc1_;
  io::QuadratureEncoder enc2_;

  ui::Knob<ui::Size> knob1_;
  ui::Knob<ui::Size> knob2_;

  io::led::Controller led_controller_;

 public:
  BreadboardEngine();

  void Tick();
  void operator()() { return Tick(); }
};

class Engine {
  // hoookay: we have 13 knobs (2 channels each), 16 buttons (1 channel each),
  // and 8:1 muxes (in hardware), so we need 6 muxes in total to account for
  // everything.
  constexpr const static size_t kMuxes = 6;

  constexpr const static size_t kHeadKnobs = 5;
  constexpr const static size_t kLfoKnobs = 8;

  io::mux::MultiGpioInMux<kMuxes> mux_;
  io::mux::ChannelScan<kMuxes, decltype(mux_)> scan_;

  Timer timer_;

  // see constructor for mux assignments
  std::array<io::QuadratureEncoder, kHeadKnobs> head_;
  io::QuadratureEncoder dry_;
  io::QuadratureEncoder wet_;

  std::array<io::QuadratureEncoder, kLfoKnobs> lfo_;
  std::array<io::Button, NUM_HEADS> head_select_;
  std::array<io::Button, NUM_LFOS> lfo_select_;

  ui::UI ui_;
  io::led::Controller leds_;
  transform::state transform_;
  transition::HeadTransitionMixer head_transitions_;

 public:
  Engine();

  const transition::Frame& Tick(const config::Config& config,
                                Samples<uint32_t> dt);
};

}  // namespace fridge::engine

#endif  // UNIT_TEST

#endif  // ENGINE_H_
