#include "libjazz/gen.hpp"

#include <limits>

namespace jazz::gen {

rc::Gen<float> ratio() {
  return rc::gen::map(
      rc::gen::inRange<int>(1, std::numeric_limits<int>::max()),
      [](int denominator) { return 1.0f / static_cast<float>(denominator); });
}

rc::Gen<float> signedRatio() {
  return rc::gen::map(rc::gen::pair(rc::gen::arbitrary<bool>(), ratio()),
                      [](std::pair<bool, float> sign_and_magnitude) {
                        auto sign = sign_and_magnitude.first;
                        auto magnitude = sign_and_magnitude.second;
                        return sign ? magnitude : -magnitude;
                      });
}

}  // namespace jazz::gen
