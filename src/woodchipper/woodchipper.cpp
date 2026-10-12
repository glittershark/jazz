#include <chrono>
#include <cmath>
#include <cstdlib>

#ifndef UNIT_TEST
#include "daisy_seed.h"
#include "io.hpp"
#include "ui.hpp"

using namespace daisy;
using namespace daisy::seed;
using namespace std::chrono_literals;

// FIXME: dedup
using namespace fridge;

DaisySeed hw;
#endif  // UNIT_TEST

// written from the scan timer IRQ, read from the main loop
static volatile int g_bits = 0;

template <typename L, typename... Inner>
class LayerApplication {
  L l_;
  LayerApplication<Inner...> i_;

 public:
  LayerApplication(L l, LayerApplication<Inner...> i) : l_(l), i_(i) {}
  float operator()(float input) { return l_.post(i_(l_.pre(input)), input); }
};

template <typename L>
class LayerApplication<L> {
  L l_;

 public:
  LayerApplication(L l) : l_(l) {}
  float operator()(float input) { return l_.post(l_.pre(input), input); }
};

template <typename L, typename... Inner>
LayerApplication<L, Inner...> apply(L l, Inner... inner) {
  return LayerApplication(l, apply(inner...));
}

template <typename L>
LayerApplication<L> apply(L l) {
  return LayerApplication(l);
}

template <typename L, typename L2>
LayerApplication<L, L2> apply(L l, L2 l2) {
  return LayerApplication(l, LayerApplication<L2>(l2));
}

class Crush {
  float bit_scale_;

 public:
  constexpr Crush(float bits) : bit_scale_(powf(2.f, bits)) {}

  float pre(float sample) { return roundf(sample * bit_scale_) / bit_scale_; }

  float post(float sample, float) { return sample; }
};

class Warp {
  float power_;

 public:
  constexpr Warp(float power) : power_(power) {}

  float pre(float sample) { return powf(fabsf(sample), power_); }

  float post(float sample, float orig) {
    return copysignf(powf(sample, 1 / power_), orig);
  }
};

class Decimate {
  size_t skip_;
  float output_;
  size_t count_;

 public:
  constexpr Decimate(size_t skip_) : skip_(skip_), output_(0.f), count_(0) {}

  float pre(float sample) { return sample; }

  float post(float sample, float) {
    if (count_ == 0) {
      count_ = skip_;
      output_ = sample;
    } else {
      --count_;
    }

    return output_;
  }
};

class LogWarp {
  float log_base_;

 public:
  constexpr LogWarp(float base) : log_base_(logf(base)) {}

  float pre(float sample) { return logf(fabsf(sample)) / log_base_; }

  float post(float sample, float orig) {
    return copysignf(expf(sample * log_base_), orig);
  }
};

float woodchipper(float sample) {
  static auto effect =
      apply(Decimate(10), LogWarp(3.3f), Warp(0.7f), Crush(0.2f));
  return effect(sample);
}

#ifndef UNIT_TEST
void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out,
                   size_t size) {
  for (size_t i = 0; i < size; i++) {
    out[0][i] = woodchipper(in[0][i]);
  }
}

/**
 * Calls a given callback at some microsecond period, consuming a timer
 * peripheral to do so. Used for stuff that, you know, needs regular, even
 * updates.
 *
 * FIXME: dedup
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

class Engine {
 public:
  constexpr const static size_t kMuxes = 8;

  Engine();

 private:
  io::mux::MultiGpioInMux<kMuxes> mux_;
  io::mux::ChannelScan<8U, decltype(mux_)> scan_;

  Timer timer_;

  io::PushbuttonQuadratureEncoder bits_;

  // leds_ must be declared before ui_, which writes to it during init
  io::led::Controller leds_;
  ui::UI ui_;
};

Timer::Timer(daisy::TimerHandle::Config::Peripheral timer, Callback<> callback,
             std::chrono::microseconds period)
    : callback_(callback), period_(period) {
  daisy::TimerHandle::Config timer_config;
  timer_config.periph = timer;
  timer_config.enable_irq = true;

  timer_.Init(timer_config);
  timer_.SetCallback(Timer::timer_callback_, this);
  timer_.SetPeriod((period_.count() * timer_.GetFreq()) / 1'000'000);
}

namespace {
constexpr const size_t kSW0 = 0;
constexpr const size_t kSW1 = 1;
constexpr const size_t kKNOB_A0 = 2;
constexpr const size_t kKNOB_B0 = 3;
constexpr const size_t kKNOB_A1 = 4;
constexpr const size_t kKNOB_B1 = 5;
constexpr const size_t kKNOB_S0 = 6;
constexpr const size_t kKNOB_S1 = 7;

enum class KnobBank { K0, K1 };

static size_t knob_a(KnobBank bank) {
  switch (bank) {
  case KnobBank::K0:
    return kKNOB_A0;
  case KnobBank::K1:
    return kKNOB_A1;
  default:
    assert(false);
  }
}

static size_t knob_b(KnobBank bank) {
  switch (bank) {
  case KnobBank::K0:
    return kKNOB_B0;
  case KnobBank::K1:
    return kKNOB_B1;
  default:
    assert(false);
  }
}

static size_t knob_s(KnobBank bank) {
  switch (bank) {
  case KnobBank::K0:
    return kKNOB_S0;
  case KnobBank::K1:
    return kKNOB_S1;
  default:
    assert(false);
  }
}

static io::PushbuttonQuadratureEncoder knob(
    io::mux::MultiGpioInMux<Engine::kMuxes>* mux, KnobBank bank,
    size_t channel) {
  return io::PushbuttonQuadratureEncoder({
      .a = mux->channel(knob_a(bank), channel),
      .b = mux->channel(knob_b(bank), channel),
      .button = mux->channel(knob_s(bank), channel),
  });
}

}  // namespace

Engine* engine = nullptr;
alignas(Engine) char engine_memory[sizeof(Engine)];

void on_knob_update(ui::UI* ui) {
  g_bits = ui->knob().Get();
}

Engine::Engine()
    : mux_({
          /* 0 =*/io::mux::GpioInMux(D3),   // SW0 in the schematic
          /* 1 =*/io::mux::GpioInMux(D4),   // SW1 in the schematic
          /* 2 =*/io::mux::GpioInMux(D5),   // KNOB_A0 in the schematic
          /* 3 =*/io::mux::GpioInMux(D6),   // KNOB_B0 in the schematic
          /* 4 =*/io::mux::GpioInMux(D7),   // KNOB_A1 in the schematic
          /* 5 =*/io::mux::GpioInMux(D8),   // KNOB_B1 in the schematic
          /* 6 =*/io::mux::GpioInMux(D9),   // KNOB_S0 in the schematic
          /* 7 =*/io::mux::GpioInMux(D10),  // KNOB_S1 in the schematic
      }),
      scan_(io::mux::Address(D0, D1, D2), &mux_),
      timer_(TimerHandle::Config::Peripheral::TIM_3, scan_.GetCallback()),
      bits_(knob(&mux_, KnobBank::K0, 0)),
      leds_({.interrupt = D13, .shutdown = D14}),
      ui_(leds_) {
  bits_.OnChange(ui_.knob().GetCallback());
  ui_.knob().OnChange(TypedCallback<ui::UI>{
      .callback = on_knob_update,
      .data = &ui_,
  });

  timer_.Start();
}

int main() {
  hw.Configure();
  hw.Init();
  hw.SetAudioBlockSize(4);
  hw.StartLog(false);

  // after hw.Init(): Engine sets up GPIO, I2C and timers, which need clocks
  ::engine = new (engine_memory) Engine();

  AdcChannelConfig adcConfig;
  adcConfig.InitSingle(hw.GetPin(21));
  hw.adc.Init(&adcConfig, 1);
  hw.adc.Start();

  hw.StartAudio(AudioCallback);

  int last_bits = g_bits;
  hw.PrintLine("bits = %d", last_bits);
  for (;;) {
    // print from here rather than the callback, which runs in the timer IRQ
    const int bits = g_bits;
    if (bits != last_bits) {
      last_bits = bits;
      hw.PrintLine("bits = %d", bits);
    }
  }
}
#endif  // UNIT_TEST
