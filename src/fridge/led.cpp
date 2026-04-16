#include "led.hpp"

#include "per/i2c.h"

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

  // set command register to point at frame 9 (device control)
  uint8_t data = 0x0b;
  i2c_.WriteDataAtAddress(address_, 0xfd, 1, &data, 1, timeout_);

  // switch the device off
  data = 0b0000'0000;
  i2c_.WriteDataAtAddress(address_, 0x0a, 1, &data, 1, timeout_);

  // wait for some reason
  daisy::System::Delay(10000);

  // switch the device on
  data = 0b0000'0001;
  i2c_.WriteDataAtAddress(address_, 0x0a, 1, &data, 1, timeout_);

  // set command register to point at frame 0
  data = 0x0;
  i2c_.WriteDataAtAddress(address_, 0xfd, 1, &data, 1, timeout_);

  // clear out every single register in the page
  data = 0x0;
  for (uint16_t reg = 0x0; reg <= 0xb3; ++reg) {
    i2c_.WriteDataAtAddress(address_, reg, 1, &data, 1, timeout_);
  }
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
