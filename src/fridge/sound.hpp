#ifndef SOUND_H_
#define SOUND_H_

#include <algorithm>
#include <array>
#include <cstddef>

#include "config.hpp"
#include "constants.hpp"
#include "head_transition.hpp"
#include "libjazz/slab.hpp"

namespace fridge::sound {

struct Update {
  enum Kind { kErase, kWrite } kind;
  size_t finished_at;
  float value;
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
    Update* last_update;   // tail pointer for O(1) append
    Update* erase_update;  // pointer to the single pending erase, or nullptr

    SampleWithUpdates(float sample = 0.0f, Update* first_update = nullptr)
        : sample(sample),
          first_update(first_update),
          last_update(first_update),
          erase_update(nullptr) {}
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

  ~BufferValue();

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

  /** Call before freeing a node that has been unlinked from the list */
  void OnUpdateFreed(Update* freed_update);

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

class Sound {
 private:
  static constexpr const size_t global_clock_max_ = SIZE_MAX - 1;
  size_t global_clock_ = 0;

  std::array<IndicesToUpdate*, FADE_TIME> indices_to_update_{};
  std::array<BufferValue, BUFFER_LEN> buffer_;

  /** Perform pre-tick housekeeping */
  void PreHousekeeping(size_t clock_time);

 public:
  ~Sound();

 private:
  /** Apply finished updates to a buffer index */
  void DoUpdate(size_t index);

  /**
   * Read the value from the buffer at `position` in the buffer.
   */
  float Read(size_t position);

  /**
   * Write a value `sample` to the buffer at `position`
   */
  void Write(size_t position, float sample);

  /**
   * Erase the value in the buffer at `position` by multiplying it by `amount`
   * (which should be between 0 and 1)
   * */
  void Erase(size_t position, float amount);
  float ApplyHead(const fridge::config::Head& head, float sample);

 public:
  /**
   * Process an audio sample. Moves the internal clock forwards by 1
   */
  float ProcessSample(const fridge::config::Config& config, float sample);
  float ProcessSample(const fridge::transition::Frame& frame, float sample);
};

}  // namespace fridge::sound

#endif  // SOUND_H_
