#include "engine.hpp"

#include "daisy_seed.h"
#include "io.hpp"
#include "per/tim.h"
#include "ui.hpp"

namespace fridge::engine {

Engine::Engine()
    // yup, these are just a lot of magic numbers
    : encoders_({
          io::mux::GpioInMux(D4),
          io::mux::GpioInMux(D3),

      }),
      scan_(io::mux::channel_scan::make<8>(
          io::mux::Address(
              /* a = */ D15,
              /* b = */ D16,
              /* c = */ D17),
          TimerHandle::Config::Peripheral::TIM_3, encoders_)),
      head_({
          .position = QuadratureEncoder(encoders_.channel(0, 2),
                                        encoders_.channel(1, 2)),
          .write_amount = QuadratureEncoder(encoders_.channel(0, 0),
                                            encoders_.channel(1, 0)),

      }) {
  head_.position.OnChange(ui_.head.position.Callback());
  head_.write_amount.OnChange(ui_.head.write_amount.Callback());
  // TODO(nausicaa): the rest of the knobs lol
}

}  // namespace fridge::engine
