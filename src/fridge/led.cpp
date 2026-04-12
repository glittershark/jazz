#include "led.hpp"

#ifndef UNIT_TEST

using namespace fridge::driver::led;

#include <cassert>

#include "daisy_seed.h"

Controller::Controller(uint16_t address) : address(address), timeout(1000) {
  I2CHandle::Config c;
  c.periph = I2CHandle::Config::Peripheral::I2C_1;
  c.mode = I2CHandle::Config::Mode::I2C_MASTER;
  c.speed = I2CHandle::Config::Speed::I2C_400KHZ;
  c.pin_config = {
      .scl = daisy::seed::D11,
      .sda = daisy::seed::D12,
  };

  assert(i2c.Init(c) == I2CHandle::Result::OK);

  // TODO(nausicaa): chip-specific register initialization
}

Address Controller::Led::OnAddress() {
  return 0x00 + (y << 1) + (matrix == Matrix::A ? 0 : 1);
}

uint8_t Controller::Led::OnOffset() {
  return x;  // lol
}

Address Controller::Led::PwmAddress() {
  return 0x24 + x + (0x10 * y) + (matrix == Matrix::A ? 0 : 0x08);
}

void Controller::Led::On(bool on) {
  uint8_t row;

  c.i2c.ReadDataAtAddress(c.address, OnAddress(), 1, &row, 1, c.timeout);

  uint8_t bit = 1 << OnOffset();
  row = on ? row | bit : row & ~bit;

  c.i2c.WriteDataAtAddress(c.address, OnAddress(), 1, &row, 1, c.timeout);
}

void Controller::Led::Pwm(uint8_t duty) {
  c.i2c.WriteDataAtAddress(c.address, PwmAddress(), 1, &duty, 1, c.timeout);
}

#endif  // UNIT_TEST
