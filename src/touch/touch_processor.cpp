#include "touch_processor.hpp"

#include <cmath>

namespace touch {

Processor::Processor()
    // TODO(nausicaa): make this thing a bit more principled, since these
    // coefficients really depend on the sample rate
    : filter_(
          {
              1.036797740838754e-02,
              -3.599350201178753e-02,
              5.168856017752318e-02,
              -3.599350201178754e-02,
              1.036797740838754e-02,
          },
          {
              -3.775615470141257,
              5.388911442196438,
              -3.444450284468981,
              0.831645207796911,
          }),
      spikes_(1),
      zero_crossings_(1) {}

float Processor::Process(float sample) {
  history_.Push(filter_.Filter(sample));
  velocity_.Push((history_[0] - history_[1]) * SAMPLE_RATE);
  acceleration_.Push((velocity_[0] - velocity_[1]) * SAMPLE_RATE);

  switch (LatestFeature()) {
  case Feature::Peak:
  case Feature::Trough:
    spikes_ += 1;
    break;
  case Feature::ZeroCrossing:
    zero_crossings_ += 1;
  }

  return std::copysign(std::pow(sample, 2 * (spikes_ / zero_crossings_)),
                       sample);
}

static bool HasZeroCrossing(const Processor::HistoryBuffer& buf) {
  return buf[0] == 0 || buf[0] * buf[1] < 0;
}

Feature Processor::LatestFeature() {
  // if the product of any of these is negative, we crossed zero between them
  const bool h_zero = HasZeroCrossing(history_);
  const bool v_zero = HasZeroCrossing(velocity_);
  const bool a_zero = HasZeroCrossing(acceleration_);

  if (h_zero && !v_zero) {
    return Feature::ZeroCrossing;
  }

  if (a_zero) {
    return Feature::Inflection;
  }

  // classic first- and second-derivative tests
  if (v_zero) {
    if (acceleration_[0] > 0) {
      return Feature::Trough;
    } else if (acceleration_[0] < 0) {
      return Feature::Peak;
    }
  }

  return Feature::None;
}

}  // namespace touch
