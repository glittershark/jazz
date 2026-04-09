#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "libjazz/slab.hpp"
#include "libjazz/value.hpp"

// Constants
constexpr const size_t BUFFER_LEN = 22050 * 1; /* 1 second */
constexpr const size_t MAX_FADE_TIME = 128;
constexpr const size_t NUM_HEADS = 4;
constexpr const size_t UPDATE_CAP =
    ((NUM_HEADS + 1) * ((MAX_FADE_TIME + 1) * 2));

struct RandomFade {
  size_t countdown;
  size_t old_index;
  // TODO(aspen): Once we switch directions, this will also need to remember the
  // old direction
};

struct RandomHead {
  size_t grain_size = 1000;
  size_t grain_remaining = grain_size;
  std::optional<RandomFade> fade = std::nullopt;
};

struct Head {
  enum class Kind { kRead, kErase, kWrite } kind;
  enum class Direction { kForwards, kBackwards } direction;
  size_t index;
  float value;
  size_t step = 1;
  std::optional<RandomHead> random;

  void Process(float sample);
};

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

  static_assert(std::input_iterator<Iterator>);
};

static Slab<Update, UPDATE_CAP> UPDATES;

class IndicesToUpdate {
 private:
  size_t index_;
  IndicesToUpdate* next_ = nullptr;
  static Slab<IndicesToUpdate, UPDATE_CAP> SLAB;

 public:
  IndicesToUpdate() : index_(0) {}
  IndicesToUpdate(size_t index) : index_(index) {}

  static void Prepend(IndicesToUpdate** head, size_t index) {
    auto new_head = SLAB.Alloc(index);
    new_head->next_ = *head;
    *head = new_head;
  }

  size_t index() const { return index_; }

  IndicesToUpdate* next() const { return next_; }

  class Iterator {
    const IndicesToUpdate* cur_;

   public:
    using difference_type = std::ptrdiff_t;
    using value_type = IndicesToUpdate;

    Iterator(const IndicesToUpdate* cur) : cur_(cur) {}

    const IndicesToUpdate& operator*() const { return *cur_; }
    void operator++(int) { ++*this; };
    Iterator& operator++() {
      cur_ = cur_->next_;
      return *this;
    };

    bool operator!=(Iterator& other) const { return other.cur_ != cur_; }

    Iterator begin() const { return *this; }
    Iterator end() const { return Iterator(nullptr); }
  };
  friend Iterator;

  class DrainingIterator {
    IndicesToUpdate* cur_;

   public:
    using difference_type = std::ptrdiff_t;
    using value_type = IndicesToUpdate;

    DrainingIterator(IndicesToUpdate* cur) : cur_(cur) {}

    const IndicesToUpdate& operator*() const { return *cur_; }
    void operator++(int) { ++*this; };
    DrainingIterator& operator++() {
      auto old = cur_;
      cur_ = cur_->next_;
      IndicesToUpdate::SLAB.Free(old);
      return *this;
    };

    bool operator!=(DrainingIterator& other) const {
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

class BufferValue {
 public:
  struct SampleWithUpdates {
    float sample;
    Update* first_update;

    SampleWithUpdates(float sample = 0.0f, Update* first_update = nullptr)
        : sample(sample), first_update(first_update) {}
  };
  static Slab<SampleWithUpdates, UPDATE_CAP> SAMPLES;
  using SlabPtr = decltype(SAMPLES)::Ptr<&SAMPLES>;

  union {
    float float_;
    SlabPtr ptr_;
    std::ptrdiff_t pd_;
  };

  static const size_t INT_TAG = 1ull << (sizeof(void*) * 8 - 1);

 protected:
  SlabPtr asSampleWithUpdates() {
    assert(isSampleWithUpdates());
    return SlabPtr::FromInt(pd_ & ~INT_TAG);
  }
  float asSample() { return float_ - 1.0f; }

 public:
  BufferValue(SlabPtr ptr) : ptr_(ptr) { pd_ |= INT_TAG; }
  BufferValue(float sample) : float_(std::clamp(sample, -1.0f, 1.0f) + 1.0) {
    pd_ &= ~INT_TAG;
    assert(!isSampleWithUpdates());
  }
  BufferValue() : BufferValue(0.0f) {}

  ~BufferValue() {
    if (isSampleWithUpdates()) {
      SAMPLES.FreePtr(asSampleWithUpdates());
    }
  }

  inline bool isSampleWithUpdates() const { return (pd_ & INT_TAG) == INT_TAG; }
  inline bool isSample() const { return !isSampleWithUpdates(); }

  float sample() {
    if (isSampleWithUpdates()) {
      return asSampleWithUpdates()->sample;
    } else {
      return asSample();
    }
  }

  void setSample(float sample) {
    if (isSampleWithUpdates()) {
      asSampleWithUpdates()->sample = sample;
    } else {
      float_ = std::clamp(sample, -1.0f, 1.0f) + 1.0f;
      pd_ &= ~INT_TAG;
    }
  }

  Update* PushBack(Update&& update);

  Update** FirstUpdate();

  void Housekeep();

  class DrainingIterator {
    enum { kHead, kUpdate } kind_;
    union {
      SampleWithUpdates* head_;
      Update* update_;
    };

   public:
    using difference_type = std::ptrdiff_t;
    using value_type = IndicesToUpdate;

    DrainingIterator(SampleWithUpdates* head) : kind_(kHead), head_(head) {}

    const Update& operator*() const {
      if (kind_ == kHead) {
        return *head_->first_update;
      } else {
        return *update_;
      }
    }

    void operator++(int) { ++*this; };
    DrainingIterator& operator++() {
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

    bool operator!=(DrainingIterator& other) const {
      return other.head_ == head_;
    }

    DrainingIterator begin() const { return *this; }
    DrainingIterator end() const { return DrainingIterator(nullptr); }
  };
  friend DrainingIterator;
};

class Granular {
 private:
  std::array<IndicesToUpdate*, MAX_FADE_TIME> indices_to_update_;
  size_t global_clock_max_ = SIZE_MAX;
  size_t global_clock_ = 0;

 public:
  std::array<BufferValue, BUFFER_LEN> BUFFER;

  void Erase(size_t index, size_t clock_time, float value,
             size_t samples = MAX_FADE_TIME);
  void Write(size_t index, size_t clock_time, float value,
             size_t samples = MAX_FADE_TIME);
  void DoUpdate(size_t index, size_t clock_time);
  float Read(size_t index, size_t clock_time);
  void PreHousekeeping(size_t clock_time);

  ~Granular() = default;

  size_t global_clock_max() const { return global_clock_max_; }

  template <size_t HEADS>
  float FullCycle(std::array<Head, HEADS> heads) {
    PreHousekeeping(global_clock_);
    float wet_signal = 0.f;

    for (auto&& head : heads) {
      switch (head.kind) {
      case Head::Kind::kRead: {
        // TODO(aspen): Refactor; pull out a helper function pls
        auto value = Read(head.index, global_clock_);
        if (head.random && head.random->fade) {
          auto at_fade = Read(head.random->fade->old_index, global_clock_);
          float fade_amt =
              ((float)(MAX_FADE_TIME - head.random->fade->countdown) /
               (float)MAX_FADE_TIME);
          value *= fade_amt;
          value += (at_fade * (1.0f - fade_amt));
        }

        wet_signal += value * head.value;
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

    global_clock_ = (global_clock_ + 1) % global_clock_max_;
    return wet_signal;
  }
};
