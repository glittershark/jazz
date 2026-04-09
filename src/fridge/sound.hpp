#ifndef SOUND_H_
#define SOUND_H_

#include <cstddef>

namespace fridge::sound {

struct Update {
  enum Kind { kErase, kWrite } kind;
  size_t finished_at;
  float value;
  size_t samples;
  Update* next_;

  class Iterator {
    const Update* cur;

   public:
    using difference_type = std::ptrdiff_t;
    using value_type = Update;

    Iterator(const Update* cur) : cur(cur) {}

    const Update& operator*() const { return *cur; }
    void operator++(int) { ++*this; };
    Iterator& operator++() {
      cur = cur->next_;
      return *this;
    };

    bool operator!=(Iterator& other) const { return other.cur != cur; }
  };
  friend Iterator;

  Iterator begin() const { return Iterator(this); }
  Iterator end() const { return Iterator(nullptr); }
};

}  // namespace fridge::sound

#endif  // SOUND_H_
