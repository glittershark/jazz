#ifndef UTIL_H_
#define UTIL_H_

template <typename T>
struct Callback {
  void (*callback)(void*, T);
  void* data;
};

#endif  // UTIL_H_
