#ifndef VALUE_H_
#define VALUE_H_

#include <cassert>
#include <cstdint>

// Platform-specific floating point type
#if UINTPTR_MAX == 0xFFFFFFFF
// 32-bit platform
using FloatType = float;
using StorageType = uint32_t;
#elif UINTPTR_MAX == 0xFFFFFFFFFFFFFFFF
// 64-bit platform
using FloatType = double;
using StorageType = uint64_t;
#else
#error "Unsupported platform"
#endif

inline bool isNegativeZero(float x) {
  return (int)x == 0 && (*(uint32_t *)&x >> 31) == 1;
}

inline bool isNegativeZero(double x) {
  return (long long)x == 0 && (*(uint64_t *)&x >> 63) == 1;
}

class Value {
private:
  union {
    FloatType asFloat;
    StorageType asBits;
  };

  static const StorageType MaxFloat =
      sizeof(void *) == 4 ? 0xfff80000ULL : 0xfff8000000000000ULL;
  static const StorageType PtrTag =
      sizeof(void *) == 4 ? 0xfffa0000ULL : 0xfffa000000000000ULL;

public:
  inline Value() : Value(0.f) {}

  inline Value(const FloatType number) { asFloat = number; }

#if UINTPTR_MAX == 0xFFFFFFFF
  // On 32-bit platforms, allow construction from double (converting to float)
  inline Value(const double number) : Value(static_cast<float>(number)) {}
#else
  // On 64-bit platforms, allow construction from float (converting to double)
  inline Value(const float number) : Value(static_cast<double>(number)) {}
#endif

  inline Value(void *pointer) {
    uintptr_t ptr_val = reinterpret_cast<uintptr_t>(pointer);
    assert((ptr_val & PtrTag) == 0);

#if UINTPTR_MAX == 0xFFFFFFFF
    asBits = static_cast<StorageType>(ptr_val) | PtrTag;
#else
    // On 64-bit, ensure pointer fits in available bits
    assert(ptr_val < (StorageType(1) << 48)); // Use 48 bits for pointer
    asBits = static_cast<StorageType>(ptr_val) | PtrTag;
#endif
  }

  inline bool isFloat() const { return asBits < MaxFloat; }
  inline bool isPointer() const { return (asBits & PtrTag) == PtrTag; }

  inline FloatType &getFloat() {
    assert(isFloat());
    return asFloat;
  }

  inline void *getPointer() const {
    assert(isPointer());
    return reinterpret_cast<void *>(asBits & ~PtrTag);
  }
};

#endif // VALUE_H_
