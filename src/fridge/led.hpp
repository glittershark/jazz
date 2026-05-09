#ifndef LED_H_
#define LED_H_

#include <cstdint>

#ifndef UNIT_TEST

#include <array>

#include "per/i2c.h"

#endif  // UNIT_TEST

namespace fridge::io::led {

#ifndef UNIT_TEST

using daisy::I2CHandle;

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
  Controller(uint8_t address = 0x74);

  Led A(uint8_t x, uint8_t y) { return Led(*this, Matrix::A, x, y); };
  Led B(uint8_t x, uint8_t y) { return Led(*this, Matrix::B, x, y); };

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

  std::array<CachedReg, 0xb3> frame_cache_;

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

#endif  // UNIT_TEST

}  // namespace fridge::io::led

#endif  // LED_H_
