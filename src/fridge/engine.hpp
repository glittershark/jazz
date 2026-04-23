#ifndef ENGINE_H_
#define ENGINE_H_

#ifndef UNIT_TEST

#include <array>

#include "config.hpp"
#include "config_transform.hpp"
#include "io.hpp"
#include "led.hpp"
#include "ui.hpp"

namespace fridge::engine {

struct Microseconds {
  uint32_t us;
};

constexpr Microseconds operator""_us(unsigned long long int us) {
  return {.us = static_cast<uint32_t>(us)};
}

template <std::size_t NumCallbacks, Microseconds Period>
class Timer {
  std::array<Callback<>, NumCallbacks> callbacks_;
  daisy::TimerHandle timer_;

  static void timer_callback_(void* this_) {
    reinterpret_cast<Timer*>(this_)->tick_();
  }

  void tick_() {
    for (auto& cb : callbacks_) {
      cb();
    }
  }

 public:
  Timer(const Timer&) = delete;
  Timer& operator=(const Timer&) = delete;

  Timer(daisy::TimerHandle::Config::Peripheral timer,
        std::array<Callback<>, NumCallbacks> callbacks)
      : callbacks_(callbacks) {
    daisy::TimerHandle::Config timer_config;
    timer_config.periph = timer;
    timer_config.enable_irq = true;

    timer_.Init(timer_config);
    timer_.SetCallback(Timer::timer_callback_, this);
    timer_.SetPeriod((Period.us * timer_.GetFreq()) / 1'000'000);
  }

  daisy::TimerHandle::Result Start() { return timer_.Start(); }
};

/**
 * The thing that makes the world go 'round!
 */
class BreadboardEngine {
  io::mux::MultiGpioInMux<2> encoders_;
  io::mux::ChannelScan<8U, decltype(encoders_)> scan_;
  Timer<1, 100_us> timer_;

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
  io::mux::MultiGpioInMux<6> mux_;
  io::mux::ChannelScan<8U, decltype(mux_)> scan_;

  Timer<1, 100_us> timer_;

  // see constructor for mux assignments
  std::array<io::QuadratureEncoder, 5> head_;
  io::QuadratureEncoder dry_;
  io::QuadratureEncoder wet_;

  std::array<io::QuadratureEncoder, 8> lfo_;
  std::array<io::Button, 8> head_select_;
  std::array<io::Button, 8> lfo_select_;

  ui::UI ui_;
  io::led::Controller leds_;
  transform::state transform_;

 public:
  Engine();

  void Tick(config::Config& config, float dt);
};

}  // namespace fridge::engine

#endif  // UNIT_TEST

#endif  // ENGINE_H_
