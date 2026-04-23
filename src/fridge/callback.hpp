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

  operator Callback<Args...>() {
    return Callback<Args...>{
        .callback = reinterpret_cast<void (*)(void*, Args...)>(callback),
        .data = static_cast<void*>(data),
    };
  }

  operator bool() { return callback != nullptr; }
};

template <typename T, typename... Args>
struct MemberCallback {
  T* this_ = nullptr;
  void (T::*callback)(Args...) = nullptr;

  void operator()(Args... args) { (this_->*callback)(args...); }

  operator bool() { return callback != nullptr; }

  operator Callback<Args...>() {
    return TypedCallback<MemberCallback, Args...>{
        .callback = MemberCallback::call_,
        .data = this,
    };
  }

 private:
  static void call_(MemberCallback* this_, Args... args) {
    this_->operator()(args...);
  }
};

#endif  // UTIL_H_
