#ifndef SLAB_H_
#define SLAB_H_

#include <cassert>
#include <cstddef>

template <typename T, const size_t len> class Slab {
private:
  union Entry {
    T val;
    size_t next;
  };

  Entry _entries[len];
  size_t _next;
  size_t _len;

public:
  inline Slab() : _next(0), _len(0) {}

  T *Alloc() {
    assert(_next < len);
    auto res = &_entries[_next];
    if (_next == _len) {
      _len++;
      _next++;
    } else {
      _next = _entries[_next].next;
    }
    new (&res->val) T;
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
