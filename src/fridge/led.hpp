#ifndef LED_H_
#define LED_H_

#include <array>
#include <cassert>
#include <cstdint>

#ifndef UNIT_TEST

#include "daisy_core.h"
#include "per/gpio.h"
#include "per/i2c.h"

#endif  // UNIT_TEST

namespace fridge::io::led {

constexpr const uint8_t kMaxX = 7;
constexpr const uint8_t kMaxY = 8;

#ifndef UNIT_TEST

using daisy::I2CHandle;

/**
 * The size of a frame register page
 *
 * A frame register is a set of addresses that store LED state, blink state, and
 * PWM state.
 *
 * See page 9 of the datasheet, table 3
 */
constexpr const size_t kFramePageSize = 0xb4;

/**
 * Driver for the IS31FL3731.
 *
 * A note on addressing: the datasheet uses a scheme (per-matrix) like C_{y-x},
 * so if you're referencing the datasheet, that's how to interpret the x-y
 * address pairs.
 */
class Controller {
  enum class Matrix { A, B };

  /**
   * Represents an address on the controller itself. Since the controller uses
   * paged ("framed", in its words) memory, this is a bit more convenient than
   * writing everything by hand.
   */
  struct Address {
    uint8_t frame;
    uint8_t reg;
  };

  struct BitAddress {
    Address addr;
    uint8_t bit;
  };

  /** Misc IO pin configuration */
  struct Pins {
    daisy::Pin interrupt;
    daisy::Pin shutdown;
  };

 public:
  class Led {
    friend class Controller;

    Controller& c_;
    Matrix matrix_;
    uint8_t x_;
    uint8_t y_;

    // note: private constructor!
    Led(Controller& c, Matrix matrix, uint8_t x, uint8_t y)
        : c_(c), matrix_(matrix), x_(x), y_(y) {}

    BitAddress OnAddress();
    Address PwmAddress();

   public:
    /**
     * Sets whether this LED is on and should be PWMed.
     */
    Led& SetOn(bool on);

    /**
     * Sets the PWM duty cycle for this LED. This only takes effect if the LED
     * is on, and will not change if the LED is switched off.
     */
    Led& SetPwm(uint8_t duty);
  };

  /**
   * Creates a new Controller (duh). `address` is set in hardware (there are a
   * few bridgeable pads on the demo board, which correspond to address setting
   * pins on the chip), so if in doubt, leave it as its default.
   */
  Controller(Pins pins, uint8_t address = 0x74);

  Led A(uint8_t x, uint8_t y) {
    assert(x <= kMaxX);
    assert(y <= kMaxY);
    return Led(*this, Matrix::A, x, y);
  };

  Led B(uint8_t x, uint8_t y) {
    assert(x <= kMaxX);
    assert(y <= kMaxY);
    return Led(*this, Matrix::B, x, y);
  };

 private:
  struct CachedReg {
    uint8_t value = 0;
    bool dirty = true;

    CachedReg& operator=(uint8_t v) {
      value = v;
      dirty = false;
      return *this;
    }
  };

  daisy::GPIO interrupt_;
  daisy::GPIO shutdown_;

  std::array<CachedReg, kFramePageSize> frame_cache_;

  I2CHandle i2c_;
  uint8_t i2c_address_;
  uint32_t timeout_;

  // TODO(nausicaa): error checking?

  uint8_t current_frame_;
  void ActivateFrame(uint8_t frame);

  uint8_t Read(Address addr);
  bool Read(BitAddress addr);

  void Write(Address addr, uint8_t value);
  void Write(BitAddress addr, bool value);
};

using Led = Controller::Led;

#else  // UNIT_TEST

class MockLed {
  bool on_ = false;
  uint8_t duty_ = 0;

 public:
  MockLed& SetOn(bool on) {
    on_ = on;
    return *this;
  };
  MockLed& SetPwm(uint8_t duty) {
    duty_ = duty;
    return *this;
  };

  bool on() const { return on_; };
  uint8_t duty() const { return duty_; }
};
using Led = MockLed;

class MockController {
  std::array<std::array<MockLed, kMaxX + 1>, kMaxY + 1> a_;
  std::array<std::array<MockLed, kMaxX + 1>, kMaxY + 1> b_;

 public:
  MockLed A(uint8_t x, uint8_t y) {
    assert(x <= kMaxX);
    assert(y <= kMaxY);
    return a_[x][y];
  }
  MockLed B(uint8_t x, uint8_t y) {
    assert(x <= kMaxX);
    assert(y <= kMaxY);
    return b_[x][y];
  }
};
using Controller = MockController;

#endif  // UNIT_TEST

}  // namespace fridge::io::led

#endif  // LED_H_
