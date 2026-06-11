#ifndef LIBJAZZ_UNITS_H_
#define LIBJAZZ_UNITS_H_

#include <cstdint>
#include <ostream>

namespace jazz::units {

#define UNIT(ty, cls, meth)                                     \
  template <typename T = ty>                                    \
  class cls {                                                   \
    ty inner_;                                                  \
                                                                \
   public:                                                      \
    explicit cls(ty inner) : inner_(inner) {}                   \
    ty& meth() { return inner_; }                               \
    const ty& meth() const { return inner_; }                   \
    cls operator+(const cls& other) const {                     \
      return cls(other.meth() + meth());                        \
    }                                                           \
    cls& operator+=(const cls& other) {                         \
      meth() += other.meth();                                   \
      return *this;                                             \
    }                                                           \
    cls operator*(const cls& other) const {                     \
      return cls(other.meth() * meth());                        \
    }                                                           \
    auto operator<=>(const cls& other) const = default;         \
    static constexpr const cls kZero = cls(0);                  \
  };                                                            \
  template <typename T = ty>                                    \
  std::ostream& operator<<(std::ostream& os, const cls<T>& x) { \
    return os << x.meth();                                      \
  }

UNIT(uint32_t, Samples, samples);

}  // namespace jazz::units

#endif  // LIBJAZZ_UNITS_H_
