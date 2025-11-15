#ifndef SLAB_H_
#define SLAB_H_

#include <cassert>
#include <cstddef>
#include <utility>

template <typename T, const size_t len> class Slab {
private:
  union Entry {
    T val;
    size_t next;

    ~Entry() {}
  };

  Entry _entries[len];
  size_t _next;
  size_t _len;

public:
  inline Slab() : _next(0), _len(0), _entries{} {}

  // TODO: it would be nice to destruct all the non-empty elements in ~Slab, but
  // that requires either stack space linear in the number of elements, or
  // bubble-sorting the freelist. Let's worry about this later, as we don't
  // currently put anything in here that's non-trivial.

  template <class... Args> T *Alloc(Args &&...args) {
    assert(_next < len);
    auto res = &_entries[_next];
    if (_next == _len) {
      _len++;
      _next++;
    } else {
      _next = _entries[_next].next;
    }
    new (&res->val) T(std::forward<Args &&>(args)...);
    return &res->val;
  }

  void Free(T *ptr) {
    ptr->~T();
    auto entry = reinterpret_cast<Entry *>(ptr);
    assert(entry >= _entries && entry <= &_entries[len - 1]);
    entry->next = _next;
    _next = entry - _entries;
  }
};

#endif // SLAB_H_
