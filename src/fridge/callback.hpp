#ifndef UTIL_H_
#define UTIL_H_

template <typename T, typename... Args>
struct TypedCallback;

template <typename... Args>
using Callback = TypedCallback<void, Args...>;

template <typename T, typename... Args>
struct TypedCallback {
  void (*callback)(T*, Args... args) = nullptr;
  T* data = nullptr;

  void operator()(Args... args) {
    if (callback) {
      callback(data, args...);
    }
  }

  operator bool() { return callback != nullptr; }

  operator Callback<Args...>() {
    return Callback<Args...>{
        .callback = static_cast<void (*)(void*, Args...)>(callback),
        .data = static_cast<void*>(data),
    };
  }
};

#endif  // UTIL_H_
