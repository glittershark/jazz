#ifndef ENGINE_H_
#define ENGINE_H_

#include <array>

#include "io.hpp"
#include "ui.hpp"

namespace fridge::engine {

using io::QuadratureEncoder;

class Engine {
  io::mux::MultiGpioInMux<2> encoders_;
  io::mux::ChannelScan<8, decltype(encoders_)> scan_;

  ui::UI ui_;

  struct {
    QuadratureEncoder position;
    QuadratureEncoder write_amount;
    // QuadratureEncoder read_amount;
    // QuadratureEncoder erase_amount;
    // QuadratureEncoder feedback;
  } head_;

  struct {
    // QuadratureEncoder range;
    // QuadratureEncoder max_grain_size;
    // QuadratureEncoder min_grain_size;
    // QuadratureEncoder reverse_chance;
    // QuadratureEncoder teleport_chance;
    // QuadratureEncoder pitch_shift_chance;
    // QuadratureEncoder low_octave_chance;
    // QuadratureEncoder high_octave_chance;
  } lfo_;

 public:
  Engine();
};

}  // namespace fridge::engine

#endif  // ENGINE_H_
