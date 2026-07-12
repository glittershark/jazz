#include "led.hpp"

#ifndef UNIT_TEST

#include "per/gpio.h"
using namespace fridge::io::led;

#include <cassert>

#include "daisy_seed.h"

Controller::Controller(Pins pins, uint8_t address)
    : i2c_address_(address), timeout_(1000), current_frame_(0xff) {
  // for our purposes, can't use an address greater than 0x7F
  assert(address < 0x80);

  interrupt_.Init(pins.interrupt, daisy::GPIO::Mode::INPUT,
                  daisy::GPIO::Pull::PULLUP);
  shutdown_.Init(pins.shutdown, daisy::GPIO::Mode::OUTPUT);
  shutdown_.Write(true);

  I2CHandle::Config c;
  c.periph = I2CHandle::Config::Peripheral::I2C_1;
  c.mode = I2CHandle::Config::Mode::I2C_MASTER;
  c.speed = I2CHandle::Config::Speed::I2C_400KHZ;
  c.pin_config = {
      .scl = daisy::seed::D11,
      .sda = daisy::seed::D12,
  };

  assert(i2c_.Init(c) == I2CHandle::Result::OK);

  // clear out every register in frame 0; most frames have extent 0xb3 (see page
  // 9, table 3) and we're only using frame 0.
  for (uint8_t reg = 0x0; reg < kFramePageSize; ++reg) {
    Write({.frame = 0x0, .reg = reg}, 0);
  }

  // set the low-order bit of the shutdown register (0x0a) to 1, to switch the
  // device on (see page 14, table 17)
  Write({.frame = 0x0b, .reg = 0x0a}, 0b0000'0001);
}

void Controller::ActivateFrame(uint8_t frame) {
  /*
   * this device has paged ("framed") memory: to write to it, you first write to
   * the command register to select which frame you're using, then actually
   * read/write registers in that frame. frames 0x00 through 0x09 are LED
   * control frames, and frame 0x0b is the function frame. the rest are
   * "reserved" according to the datasheet.
   */
  assert(frame <= 0x09 || frame == 0x0b);

  // set the command register (0xfd) to point at the given frame (see page 9,
  // table 2 and page 10 of the datasheet)
  if (current_frame_ != frame) {
    i2c_.WriteDataAtAddress(i2c_address_, 0xfd, 1, &frame, 1, timeout_);
    current_frame_ = frame;

    // invalidate the cache, as we just switched frames
    for (auto& reg : frame_cache_) {
      reg.dirty = true;
    }
  }
}

uint8_t Controller::Read(Address addr) {
  ActivateFrame(addr.frame);

  auto& cached = frame_cache_[addr.reg];
  if (cached.dirty) {
    uint8_t value;
    i2c_.ReadDataAtAddress(i2c_address_, addr.reg, 1, &value, 1, timeout_);
    cached = value;
  }
  return cached.value;
}

bool Controller::Read(BitAddress addr) {
  return (Read(addr.addr) >> addr.bit) & 1;
}

void Controller::Write(Address addr, uint8_t value) {
  ActivateFrame(addr.frame);

  auto& cached = frame_cache_[addr.reg];

  // only actually write if it would matter
  if (cached.dirty || cached.value != value) {
    i2c_.WriteDataAtAddress(i2c_address_, addr.reg, 1, &value, 1, timeout_);
    cached = value;
  }
}

void Controller::Write(BitAddress addr, bool state) {
  uint8_t value = Read(addr.addr);
  value = state ? value | (1 << addr.bit) : value & ~(1 << addr.bit);
  Write(addr.addr, value);
}

Controller::BitAddress Controller::Led::OnAddress() {
  return {
      .addr =
          {
              .frame = 0x0,
              // see page 11, table 7 of the datasheet for how individual LED
              // bit addresses are calculated
              .reg = static_cast<uint8_t>(0x00 | (y_ << 1) |
                                          (matrix_ == Matrix::A ? 0 : 1)),
          },
      .bit = x_,  // lol
  };
}

Controller::Address Controller::Led::PwmAddress() {
  return {
      .frame = 0x0,
      // see page 11, table 7 of the datasheet for the layout of the PWM
      // registers in memory; basically, the matrix rows are interleaved, eight
      // at a time, starting at register 0x24, resulting in rows sixteen (0x10)
      // registers long.
      .reg = static_cast<uint8_t>(0x24 + x_ + (0x10 * y_) +
                                  (matrix_ == Matrix::A ? 0 : 0x08)),
  };
}

Controller::Led& Controller::Led::SetOn(bool on) {
  c_.Write(OnAddress(), on);
  return *this;
}

Controller::Led& Controller::Led::SetPwm(uint8_t duty) {
  c_.Write(PwmAddress(), duty);
  return *this;
}

#endif  // UNIT_TEST
