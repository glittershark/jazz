#include "touch_processor.hpp"

namespace touch {

float Processor::Process(float sample) {
  buffer_.Push(sample);
  return buffer_[0];
}

}  // namespace touch
