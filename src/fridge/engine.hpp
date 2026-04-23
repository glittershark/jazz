#ifndef ENGINE_H_
#define ENGINE_H_

#ifndef UNIT_TEST

#include "io.hpp"
#include "led.hpp"
#include "ui.hpp"

namespace fridge::engine {

/**
 * The thing that makes the world go 'round!
 */
class BreadboardEngine {
  io::mux::MultiGpioInMux<2> encoders_;
  io::mux::ChannelScan<8U, decltype(encoders_)> scan_;
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

}  // namespace fridge::engine

#endif  // UNIT_TEST

#endif  // ENGINE_H_
