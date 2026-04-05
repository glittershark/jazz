#include "pwm.hpp"

#ifndef UNIT_TEST
#include "sys/system.h"
#include <cassert>

// Include HAL for direct hardware access
#include "stm32h7xx_hal.h"

namespace pwm {

using namespace daisy;

/// GPIO Helpers

// Get GPIO port from Pin
static GPIO_TypeDef *GetGpioPort(Pin pin) {
  constexpr GPIO_TypeDef *ports[] = {GPIOA, GPIOB, GPIOC, GPIOD, GPIOE, GPIOF,
                                     GPIOG, GPIOH, GPIOI, GPIOJ, GPIOK};
  if (pin.port <= PORTK) {
    return ports[pin.port];
  }
  return nullptr;
}

// Get GPIO pin mask from Pin
static uint16_t GetGpioPin(Pin pin) { return 1 << pin.pin; }

// Enable GPIO port clock
static void EnableGpioClock(Pin pin) {
  switch (pin.port) {
  case PORTA:
    __HAL_RCC_GPIOA_CLK_ENABLE();
    break;
  case PORTB:
    __HAL_RCC_GPIOB_CLK_ENABLE();
    break;
  case PORTC:
    __HAL_RCC_GPIOC_CLK_ENABLE();
    break;
  case PORTD:
    __HAL_RCC_GPIOD_CLK_ENABLE();
    break;
  case PORTE:
    __HAL_RCC_GPIOE_CLK_ENABLE();
    break;
  case PORTF:
    __HAL_RCC_GPIOF_CLK_ENABLE();
    break;
  case PORTG:
    __HAL_RCC_GPIOG_CLK_ENABLE();
    break;
  case PORTH:
    __HAL_RCC_GPIOH_CLK_ENABLE();
    break;
  case PORTI:
    __HAL_RCC_GPIOI_CLK_ENABLE();
    break;
  case PORTJ:
    __HAL_RCC_GPIOJ_CLK_ENABLE();
    break;
  case PORTK:
    __HAL_RCC_GPIOK_CLK_ENABLE();
    break;
  default:
    break;
  }
}

/// Timer Helpers

// Get the GPIO alternate function for a timer
static uint8_t GetTimerAF(TimerHandle::Config::Peripheral periph) {
  switch (periph) {
  case TimerHandle::Config::Peripheral::TIM_2:
    return GPIO_AF1_TIM2;
  case TimerHandle::Config::Peripheral::TIM_3:
    return GPIO_AF2_TIM3;
  case TimerHandle::Config::Peripheral::TIM_4:
    return GPIO_AF2_TIM4;
  case TimerHandle::Config::Peripheral::TIM_5:
    return GPIO_AF2_TIM5;
  default:
    return 0;
  }
}

// Get the TIM instance for a peripheral enum
static TIM_TypeDef *GetTimerInstance(TimerHandle::Config::Peripheral periph) {
  switch (periph) {
  case TimerHandle::Config::Peripheral::TIM_2:
    return TIM2;
  case TimerHandle::Config::Peripheral::TIM_3:
    return TIM3;
  case TimerHandle::Config::Peripheral::TIM_4:
    return TIM4;
  case TimerHandle::Config::Peripheral::TIM_5:
    return TIM5;
  default:
    return nullptr;
  }
}

// Enable the clock for a timer
static void EnableTimerClock(TimerHandle::Config::Peripheral periph) {
  switch (periph) {
  case TimerHandle::Config::Peripheral::TIM_2:
    __HAL_RCC_TIM2_CLK_ENABLE();
    break;
  case TimerHandle::Config::Peripheral::TIM_3:
    __HAL_RCC_TIM3_CLK_ENABLE();
    break;
  case TimerHandle::Config::Peripheral::TIM_4:
    __HAL_RCC_TIM4_CLK_ENABLE();
    break;
  case TimerHandle::Config::Peripheral::TIM_5:
    __HAL_RCC_TIM5_CLK_ENABLE();
    break;
  }
}

// Get HAL channel constant from channel number
static uint32_t GetHalChannel(uint8_t channel) {
  switch (channel) {
  case 1:
    return TIM_CHANNEL_1;
  case 2:
    return TIM_CHANNEL_2;
  case 3:
    return TIM_CHANNEL_3;
  case 4:
    return TIM_CHANNEL_4;
  default:
    return 0;
  }
}

/// Channel

void Channel::Set(uint8_t duty) {
  // Scale 0-255 to 0-period (255)
  *ccr_ = duty;
}

void Channel::SetFloat(float duty) {
  if (duty < 0.0f)
    duty = 0.0f;
  if (duty > 1.0f)
    duty = 1.0f;
  *ccr_ = (uint32_t)(duty * 255.0f);
}

/// Timer

// Storage for timer handles (one per timer peripheral)
static TIM_HandleTypeDef pwm_tim_handles[4];
static bool pwm_tim_initialized[4] = {false, false, false, false};

Timer::Timer(TimerHandle::Config::Peripheral periph, uint32_t frequency_hz)
    : periph_(periph) {
  int idx = static_cast<int>(periph);
  assert(idx < 4);
  assert(!pwm_tim_initialized[idx] && "Timer already in use");

  TIM_HandleTypeDef *htim = &pwm_tim_handles[idx];
  tim_handle_ = htim;

  EnableTimerClock(periph);

  htim->Instance = GetTimerInstance(periph);

  // Calculate prescaler and period for desired frequency
  // Timer clock is 2x PCLK1 (typically 200MHz)
  // We want: frequency_hz = timer_clock / ((prescaler + 1) * (period + 1))
  // For 8-bit resolution (period = 255), calculate prescaler
  uint32_t timer_clock = System::GetPClk1Freq() * 2;
  uint32_t prescaler = (timer_clock / (frequency_hz * 256)) - 1;

  htim->Init.Prescaler = prescaler;
  htim->Init.CounterMode = TIM_COUNTERMODE_UP;
  htim->Init.Period = 255; // 8-bit resolution for 0-255 duty cycle
  htim->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

  HAL_TIM_PWM_Init(htim);

  pwm_tim_initialized[idx] = true;
}

Channel Timer::InitChannel(uint8_t channel, Pin pin) {
  assert(channel >= 1 && channel <= 4);

  TIM_HandleTypeDef *htim = static_cast<TIM_HandleTypeDef *>(tim_handle_);

  // Configure GPIO for alternate function
  GPIO_TypeDef *port = GetGpioPort(pin);
  uint16_t gpio_pin = GetGpioPin(pin);

  EnableGpioClock(pin);

  GPIO_InitTypeDef gpio_init = {};
  gpio_init.Pin = gpio_pin;
  gpio_init.Mode = GPIO_MODE_AF_PP;
  gpio_init.Pull = GPIO_NOPULL;
  gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
  gpio_init.Alternate = GetTimerAF(periph_);
  HAL_GPIO_Init(port, &gpio_init);

  // Configure PWM channel
  TIM_OC_InitTypeDef oc_config = {};
  oc_config.OCMode = TIM_OCMODE_PWM1;
  oc_config.Pulse = 0; // Start at 0% duty cycle
  oc_config.OCPolarity = TIM_OCPOLARITY_HIGH;
  oc_config.OCFastMode = TIM_OCFAST_DISABLE;

  uint32_t hal_channel = GetHalChannel(channel);
  HAL_TIM_PWM_ConfigChannel(htim, &oc_config, hal_channel);
  HAL_TIM_PWM_Start(htim, hal_channel);

  // Return a Channel pointing to the correct CCR register
  volatile uint32_t *ccr;
  switch (channel) {
  case 1:
    ccr = &htim->Instance->CCR1;
    break;
  case 2:
    ccr = &htim->Instance->CCR2;
    break;
  case 3:
    ccr = &htim->Instance->CCR3;
    break;
  case 4:
    ccr = &htim->Instance->CCR4;
    break;
  default:
    ccr = &htim->Instance->CCR1;
    break;
  }

  return Channel(ccr);
}

void Timer::Start() {
  // Channels are started individually in InitChannel, so this is a no-op
  // but kept for API consistency if we want to batch-start later
}

} // namespace pwm
#endif // UNIT_TEST
