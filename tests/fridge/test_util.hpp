#ifndef TEST_UTIL_H_
#define TEST_UTIL_H_

#include <limits>

/* Unsigned absolute difference
 *
 * note: may overflow on pathological inputs
 */
template <typename T>
T udiff(T x, T y) {
  return (x > y) ? (x - y) : (y - x);
}

/* Unsigned absolute distance, modulo the range of the argument type
 *
 * e.g. udist((uint8t)0, (uint8_t)255) == 1
 */
template <typename T>
T udist(T x, T y) {
  auto diff = udiff(x, y);
  auto max = std::numeric_limits<T>::max();
  return (diff < (max / 2)) ? diff : max - diff;
}

#endif  // TEST_UTIL_H_
