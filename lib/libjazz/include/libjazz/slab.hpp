#ifndef SLAB_H_
#define SLAB_H_

#include <cassert>
#include <cstddef>
#include <memory>
#include <utility>

template <typename T, const size_t len> class Slab {
private:
  union Entry {
    T val;
    size_t next;

    ~Entry() {}
  };

  Entry m_entries[len];
  size_t m_next;
  size_t m_len;

public:
  inline Slab() : m_next(0), m_len(0), m_entries{} {}

  // TODO: it would be nice to destruct all the non-empty elements in ~Slab, but
  // that requires either stack space linear in the number of elements, or
  // bubble-sorting the freelist. Let's worry about this later, as we don't
  // currently put anything in here that's non-trivial.

  template <class... Args> T *Alloc(Args &&...args) {
    assert(m_next < len);
    auto res = &m_entries[m_next];
    if (m_next == m_len) {
      m_len++;
      m_next++;
    } else {
      m_next = m_entries[m_next].next;
    }
    new (&res->val) T(std::forward<Args &&>(args)...);
    return &res->val;
  }

  void Free(T *ptr) {
    ptr->~T();
    auto entry = reinterpret_cast<Entry *>(ptr);
    assert(entry >= m_entries && entry <= &m_entries[len - 1]);
    entry->next = m_next;
    m_next = entry - m_entries;
  }

  struct Deleter {
    friend Slab<T, len>;

  protected:
    Slab<T, len> &m_slab;
    Deleter(Slab<T, len> &slab) : m_slab(slab) {}

  public:
    void operator()(T *ptr) { m_slab.Free(ptr); }
  };

public:
  using unique_ptr = std::unique_ptr<T, Deleter>;

  template <class... Args> unique_ptr MakeUnique(Args &&...args) {
    return unique_ptr{Alloc(std::forward<Args &&>(args)...), Deleter(*this)};
  }
};

#endif // SLAB_H_
