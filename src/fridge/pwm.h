#ifndef FRIDGE_PWM_H_
#define FRIDGE_PWM_H_

#include "daisy_seed.h"
#include "per/tim.h"

namespace pwm {

using namespace daisy;

/**
 * PWM output channel.
 *
 * Represents a single PWM output on a timer channel. Get these from Timer.
 */
class Channel {
  friend class Timer;

  volatile uint32_t *ccr_;

  Channel(volatile uint32_t *ccr) : ccr_(ccr) {}

public:
  /** Set duty cycle from 0-255 */
  void Set(uint8_t duty);

  /** Set duty cycle from 0.0-1.0 */
  void SetFloat(float duty);
};

/**
 * PWM timer for generating PWM outputs.
 *
 * Wraps a hardware timer (TIM4 or TIM5) for PWM generation. Each timer has
 * 4 channels that can output PWM on specific pins:
 *
 * TIM4 (16-bit):
 *   - Channel 1: PB6 (D13)
 *   - Channel 2: PB7 (D14)
 *   - Channel 3: PB8 (D11)
 *   - Channel 4: PB9 (D12)
 *
 * TIM5 (32-bit):
 *   - Channel 1: PA0 (D25)
 *   - Channel 2: PA1 (D24)
 *   - Channel 3: PA2 (D28)
 *   - Channel 4: PA3 (D16)
 *
 * Usage:
 *   pwm::Timer pwm(TimerHandle::Config::Peripheral::TIM_4);
 *   auto ch1 = pwm.InitChannel(1, D13);  // TIM4_CH1 on PB6
 *   auto ch2 = pwm.InitChannel(2, D14);  // TIM4_CH2 on PB7
 *   pwm.Start();
 *   ch1.Set(128);  // 50% duty cycle
 */
class Timer {
  TimerHandle::Config::Peripheral periph_;
  void *tim_handle_; // TIM_HandleTypeDef*, opaque to avoid HAL in header

public:
  Timer(const Timer &) = delete;
  Timer &operator=(const Timer &) = delete;

  /**
   * Initialize a timer for PWM generation.
   *
   * @param periph Which timer to use (TIM_4 or TIM_5 recommended, TIM_3 if not
   *               used elsewhere). TIM_2 is used by the system.
   * @param frequency_hz PWM frequency in Hz. Default 1000 Hz is good for LEDs.
   */
  Timer(TimerHandle::Config::Peripheral periph, uint32_t frequency_hz = 1000);

  /**
   * Initialize a PWM channel on a specific pin.
   *
   * @param channel Timer channel 1-4
   * @param pin The pin to use. Must match the timer/channel (see class docs).
   * @return Channel object for controlling duty cycle
   */
  Channel InitChannel(uint8_t channel, Pin pin);

  /** Start PWM generation on all initialized channels */
  void Start();
};

} // namespace pwm

#endif // FRIDGE_PWM_H_
