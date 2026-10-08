#ifndef FRIDGE_CALLBACK_H_
#define FRIDGE_CALLBACK_H_

// The erased callback itself must accept void*. Casting a typed function
// pointer to this signature and calling it is undefined behavior.
template <typename... Args>
struct Callback {
  void (*callback)(void*, Args... args) = nullptr;
  void* data = nullptr;

  void operator()(Args... args) const {
    if (callback != nullptr) {
      callback(data, args...);
    }
  }

  explicit operator bool() const { return callback != nullptr; }
};

#endif
