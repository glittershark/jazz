#include "led.hpp"

#ifndef UNIT_TEST

using namespace fridge::io::led;

#include <cassert>

#include "daisy_seed.h"

Controller::Controller(uint16_t address) : address_(address), timeout_(1000) {
  I2CHandle::Config c;
  c.periph = I2CHandle::Config::Peripheral::I2C_1;
  c.mode = I2CHandle::Config::Mode::I2C_MASTER;
  c.speed = I2CHandle::Config::Speed::I2C_400KHZ;
  c.pin_config = {
      .scl = daisy::seed::D11,
      .sda = daisy::seed::D12,
  };

  assert(i2c_.Init(c) == I2CHandle::Result::OK);

  // TODO(nausicaa): chip-specific register initialization
}

Address Controller::Led::OnAddress() {
  return 0x00 + (y_ << 1) + (matrix_ == Matrix::A ? 0 : 1);
}

uint8_t Controller::Led::OnOffset() {
  return x_;  // lol
}

Address Controller::Led::PwmAddress() {
  return 0x24 + x_ + (0x10 * y_) + (matrix_ == Matrix::A ? 0 : 0x08);
}

void Controller::Led::On(bool on) {
  uint8_t row;

  c_.i2c_.ReadDataAtAddress(c_.address_, OnAddress(), 1, &row, 1, c_.timeout_);

  uint8_t bit = 1 << OnOffset();
  row = on ? row | bit : row & ~bit;

  c_.i2c_.WriteDataAtAddress(c_.address_, OnAddress(), 1, &row, 1, c_.timeout_);
}

void Controller::Led::Pwm(uint8_t duty) {
  c_.i2c_.WriteDataAtAddress(c_.address_, PwmAddress(), 1, &duty, 1,
                             c_.timeout_);
}

#endif  // UNIT_TEST
