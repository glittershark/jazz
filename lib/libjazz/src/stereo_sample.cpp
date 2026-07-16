#include "libjazz/stereo_sample.hpp"

namespace jazz {
namespace audio {

std::ostream& operator<<(std::ostream& out, const StereoSample& val) {
  return out << "StereoSample{.left =" << val.left << ", .right =" << val.right
             << "}";
}

}  // namespace audio
}  // namespace jazz
