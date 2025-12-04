#pragma once

#include "libjazz/slab.hpp"
#include "libjazz/value.hpp"
#include <array>
#include <cstddef>

// Constants
constexpr const size_t BUFFER_LEN = 44100 * 2; /* 2 seconds */
constexpr const size_t MAX_FADE_TIME = 64;
constexpr const size_t NUM_HEADS = 3;
constexpr const size_t UPDATE_CAP = (NUM_HEADS + 1) * (MAX_FADE_TIME + 1);

struct Head {
  enum Kind { kRead, kErase, kWrite } kind;
  size_t index;
  float value;
};

struct Update {
  enum Kind { kErase, kWrite } kind;
  size_t finished_at;
  float value;
  size_t samples;
  Update *next_;

  class Iterator {
    const Update *cur;

  public:
    using difference_type = std::ptrdiff_t;
    using value_type = Update;

    Iterator(const Update *cur) : cur(cur) {}

    const Update &operator*() const { return *cur; }
    void operator++(int) { ++*this; };
    Iterator &operator++() {
      cur = cur->next_;
      return *this;
    };

    bool operator!=(Iterator &other) const { return other.cur != cur; }
  };
  friend Iterator;

  Iterator begin() const { return Iterator(this); }
  Iterator end() const { return Iterator(nullptr); }

  static_assert(std::input_iterator<Iterator>);
};

static Slab<Update, UPDATE_CAP> UPDATES;

class IndicesToUpdate {
private:
  size_t index_;
  IndicesToUpdate *next_;
  static Slab<IndicesToUpdate, UPDATE_CAP> SLAB;

public:
  IndicesToUpdate(size_t index) : index_(index) {}

  static void Prepend(IndicesToUpdate **head, size_t index) {
    auto new_head = SLAB.Alloc(index);
    new_head->next_ = *head;
    *head = new_head;
  }

  size_t index() const { return index_; }

  IndicesToUpdate *next() const { return next_; }

  class Iterator {
    const IndicesToUpdate *cur_;

  public:
    using difference_type = std::ptrdiff_t;
    using value_type = IndicesToUpdate;

    Iterator(const IndicesToUpdate *cur) : cur_(cur) {}

    const IndicesToUpdate &operator*() const { return *cur_; }
    void operator++(int) { ++*this; };
    Iterator &operator++() {
      cur_ = cur_->next_;
      return *this;
    };

    bool operator!=(Iterator &other) const { return other.cur_ != cur_; }

    Iterator begin() const { return *this; }
    Iterator end() const { return Iterator(nullptr); }
  };
  friend Iterator;

  class DrainingIterator {
    IndicesToUpdate *cur_;

  public:
    using difference_type = std::ptrdiff_t;
    using value_type = IndicesToUpdate;

    DrainingIterator(IndicesToUpdate *cur) : cur_(cur) {}

    const IndicesToUpdate &operator*() const { return *cur_; }
    void operator++(int) { ++*this; };
    DrainingIterator &operator++() {
      auto old = cur_;
      cur_ = cur_->next_;
      IndicesToUpdate::SLAB.Free(old);
      return *this;
    };

    bool operator!=(DrainingIterator &other) const {
      return other.cur_ != cur_;
    }

    DrainingIterator begin() const { return *this; }
    DrainingIterator end() const { return DrainingIterator(nullptr); }
  };
  friend DrainingIterator;

  Iterator iter() { return Iterator(this); }
  DrainingIterator drain() { return DrainingIterator(this); }

  static_assert(std::input_iterator<Iterator>);
  static_assert(std::input_iterator<DrainingIterator>);
};

class BufferValue : private Value {
public:
  struct SampleWithUpdates {
    FloatType sample;
    Update *first_update;
  };
  static Slab<SampleWithUpdates, UPDATE_CAP> SAMPLES;

protected:
  SampleWithUpdates *asSampleWithUpdates() const {
    return std::bit_cast<SampleWithUpdates *>(getPointer());
  }
  FloatType &asSample() { return getFloat(); }

public:
  BufferValue(FloatType sample) : Value(sample) {}
  BufferValue() : BufferValue(0.0f) {}

  FloatType &sample() {
    if (isPointer()) {
      return asSampleWithUpdates()->sample;
    } else {
      return asSample();
    }
  }

  Update *PushBack(Update &&update) {
    if (isPointer()) {
      auto head = asSampleWithUpdates();

      if (head->first_update == nullptr) {
        head->first_update = UPDATES.Alloc(std::move(update));
        return head->first_update;
      } else {
        Update *cur = asSampleWithUpdates()->first_update;
        while (cur->next_ != nullptr) {
          cur = cur->next_;
        }
        cur->next_ = UPDATES.Alloc(std::move(update));

        return cur->next_;
      }
    } else {
      float sample = asSample();
      auto upd = UPDATES.Alloc(std::move(update));
      auto head = SAMPLES.Alloc(sample, upd);
      new (this) Value((void *)head);
      return upd;
    }
  }

  Update **FirstUpdate() const {
    if (isPointer()) {
      return &asSampleWithUpdates()->first_update;
    } else {
      return nullptr;
    }
  }

  void Housekeep() {
    if (isPointer() && asSampleWithUpdates()->first_update == nullptr) {
      auto sample_ = sample();
      SAMPLES.Free(asSampleWithUpdates());
      new (this) BufferValue(sample_);
    }
  }

  class DrainingIterator {
    enum { kHead, kUpdate } kind_;
    union {
      SampleWithUpdates *head_;
      Update *update_;
    };

  public:
    using difference_type = std::ptrdiff_t;
    using value_type = IndicesToUpdate;

    DrainingIterator(SampleWithUpdates *head) : kind_(kHead), head_(head) {}

    const Update &operator*() const {
      if (kind_ == kHead) {
        return *head_->first_update;
      } else {
        return *update_;
      }
    }

    void operator++(int) { ++*this; };
    DrainingIterator &operator++() {
      if (kind_ == kHead) {
        auto old = head_;
        kind_ = kUpdate;
        update_ = old->first_update;
        SAMPLES.Free(old);
      } else {
        auto old = update_;
        update_ = old->next_;
        UPDATES.Free(old);
      }
      return *this;
    };

    bool operator!=(DrainingIterator &other) const {
      return other.head_ == head_;
    }

    DrainingIterator begin() const { return *this; }
    DrainingIterator end() const { return DrainingIterator(nullptr); }
  };
  friend DrainingIterator;
};

class Granular {

private:
  IndicesToUpdate *indices_to_update_[MAX_FADE_TIME];
  size_t global_clock_max = 300000;
  size_t global_clock_ = 0;

public:
  BufferValue BUFFER[BUFFER_LEN];

  void Erase(size_t index, size_t clock_time, float value,
             size_t samples = MAX_FADE_TIME);
  void Write(size_t index, size_t clock_time, float value,
             size_t samples = MAX_FADE_TIME);
  void DoUpdate(size_t index, size_t clock_time);
  float Read(size_t index, size_t clock_time);
  void PreHousekeeping(size_t clock_time);

  template <size_t HEADS> float FullCycle(std::array<Head, HEADS> heads) {
    PreHousekeeping(global_clock_);
    float wet_signal = 0.f;

    for (auto &&head : heads) {
      switch (head.kind) {
      case Head::Kind::kRead: {
        wet_signal += Read(head.index, global_clock_) * head.value;
        break;
      }
      case Head::Kind::kErase: {
        Erase(head.index, global_clock_, head.value);
        break;
      }
      case Head::Kind::kWrite: {
        Write(head.index, global_clock_, head.value);
        break;
      }
      }
    }

    global_clock_ = (global_clock_ + 1 + global_clock_max) % global_clock_max;
    return wet_signal;
  }
};
