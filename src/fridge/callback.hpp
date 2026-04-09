#ifndef UTIL_H_
#define UTIL_H_

template <typename... Args>
struct Callback {
  void (*callback)(void*, Args... args) = nullptr;
  void* data = nullptr;

  void operator()(Args... args) {
    if (callback) {
      callback(data, args...);
    }
  }

  operator bool() { return callback != nullptr; }
};

#endif  // UTIL_H_
