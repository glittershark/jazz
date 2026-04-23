#ifndef ENGINE_H_
#define ENGINE_H_

#ifndef UNIT_TEST

#include "config.hpp"
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

class Engine {
  // hoookay: we have 13 knobs and 8:1 muxes (in hardware), so we need two muxes
  // in total to account for all of the knobs.
  struct Muxes {
    struct MuxBank {
      io::mux::MultiGpioInMux<2> mux;
      io::mux::ChannelScan<8U, decltype(mux)> scan;
    };

    MuxBank bank_a;
    MuxBank bank_b;
  } muxes_;

  struct Knobs {
    struct Head {
      io::QuadratureEncoder position;
      io::QuadratureEncoder write_amount;
      io::QuadratureEncoder read_amount;
      io::QuadratureEncoder erase_amount;
      io::QuadratureEncoder feedback;

      Head(io::mux::MultiGpioInMux<2>& mux, ui::Head& head);
    } head;

    // placed here because they're on the same bank as all the head knobs
    io::QuadratureEncoder dry;
    io::QuadratureEncoder wet;

    struct LFO {
      io::QuadratureEncoder range;
      io::QuadratureEncoder max_grain_size;
      io::QuadratureEncoder min_grain_size;
      io::QuadratureEncoder reverse_chance;
      io::QuadratureEncoder teleport_chance;
      io::QuadratureEncoder pitch_shift_chance;
      io::QuadratureEncoder low_octave_chance;
      io::QuadratureEncoder high_octave_chance;

      LFO(io::mux::MultiGpioInMux<2>& mux, ui::LFO& lfo);
    } lfo;

    Knobs(Muxes& muxes, ui::UI& ui);
  } knobs_;

  ui::UI ui_;
  io::led::Controller leds_;

 public:
  Engine();

  void operator()(config::Config& config) { config |= ui_; }  // lmao
};

}  // namespace fridge::engine

#endif  // UNIT_TEST

#endif  // ENGINE_H_
