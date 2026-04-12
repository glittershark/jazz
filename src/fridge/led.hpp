#ifndef LED_H_
#define LED_H_

#ifndef UNIT_TEST

#include <cstdint>

#include "per/i2c.h"

namespace fridge::driver::led {

using daisy::I2CHandle;

using Address = uint16_t;

/**
 * Driver for the IS31FL3731.
 *
 * A note on addressing: the datasheet uses a scheme (per-matrix) like C_{y-x},
 * so if you're referencing the datasheet, that's how to interpret the x-y
 * address pairs.
 */
class Controller {
  enum class Matrix {
    A,
    B,
  };

 public:
  class Led {
    friend class Controller;

    Controller& c;
    Matrix matrix;
    uint8_t x;
    uint8_t y;

    // note: private constructor!
    Led(Controller& c, Matrix matrix, uint8_t x, uint8_t y)
        : c(c), matrix(matrix), x(x), y(y) {}

    Address OnAddress();
    uint8_t OnOffset();

    Address PwmAddress();

   public:
    void On(bool on);
    void Pwm(uint8_t duty);
  };

  Controller(uint16_t address = 0x75);

  Led A(uint8_t x, uint8_t y) { return Led(*this, Matrix::A, x, y); };
  Led B(uint8_t x, uint8_t y) { return Led(*this, Matrix::B, x, y); };

 private:
  I2CHandle i2c;
  uint16_t address;
  uint32_t timeout;
};

}  // namespace fridge::driver::led

#endif  // UNIT_TEST

#endif  // LED_H_
