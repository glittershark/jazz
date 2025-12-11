#ifndef SLAB_H_
#define SLAB_H_

#include <cassert>
#include <cstddef>
#include <memory>
#include <utility>

template <typename T, const size_t CAP> class Slab {
private:
  union Entry {
    T val;
    size_t next;

    ~Entry() {}
  };

  Entry entries_[CAP];
  size_t next_;
  size_t len_;

public:
  inline Slab() : entries_{}, next_(0), len_(0) {}

  // TODO: it would be nice to destruct all the non-empty elements in ~Slab, but
  // that requires either stack space linear in the number of elements, or
  // bubble-sorting the freelist. Let's worry about this later, as we don't
  // currently put anything in here that's non-trivial.

  template <class... Args> T *Alloc(Args &&...args) {
    assert(next_ < CAP);
    auto res = &entries_[next_];
    if (next_ == len_) {
      len_++;
      next_++;
    } else {
      next_ = entries_[next_].next;
    }
    new (&res->val) T(std::forward<Args &&>(args)...);
    return &res->val;
  }

  void Free(T *ptr) {
    ptr->~T();
    auto entry = reinterpret_cast<Entry *>(ptr);
    assert(entry >= entries_ && entry <= &entries_[CAP - 1]);
    entry->next = next_;
    next_ = entry - entries_;
  }

  template <Slab<T, CAP> *SLAB> class Ptr {
    std::ptrdiff_t idx_;
    friend Slab;

  protected:
    Ptr(size_t idx) : idx_(idx) {}

  public:
    const size_t AsInt() const { return idx_; }
    static Ptr FromInt(size_t idx) { return Ptr(idx); }
    const bool operator==(Ptr other) const { return idx_ == other.idx_; }
    T *get() { return SLAB->Deref(*this); }
    T &operator*() { return *SLAB->Deref(*this); }
    T *operator->() { return get(); }
  };

  template <Slab<T, CAP> *SLAB> Ptr<SLAB> Get(T *ptr) const {
    return reinterpret_cast<Entry *>(ptr) - entries_;
  }

  template <Slab<T, CAP> *SLAB, class... Args>
  Ptr<SLAB> AllocPtr(Args &&...args) {
    auto ptr = Alloc(args...);
    return Get<SLAB>(ptr);
  }

  template <Slab<T, CAP> *SLAB> T *Deref(Ptr<SLAB> index) {
    return &entries_[index.idx_].val;
  }

  template <Slab<T, CAP> *SLAB> void FreePtr(Ptr<SLAB> index) {
    Free(Deref(index));
  }

  struct Deleter {
    friend Slab<T, CAP>;

  protected:
    Slab<T, CAP> &m_slab;
    Deleter(Slab<T, CAP> &slab) : m_slab(slab) {}

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
