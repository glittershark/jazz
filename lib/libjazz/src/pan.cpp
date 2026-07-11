#include "libjazz/pan.hpp"

#include <iostream>

namespace jazz::audio {

std::ostream& operator<<(std::ostream& out, const Pan& val) {
  return out << "Pan(" << val.pan() << ")";
}

std::ostream& operator<<(std::ostream& out, const Pan::Channels& val) {
  return out << "Pan::Channels{.left =" << val.left
             << ", .right = " << val.right << "}";
}

}  // namespace jazz::audio
