#ifndef SOUND_H_
#define SOUND_H_

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstddef>

#include "config.hpp"
#include "constants.hpp"
#include "libjazz/slab.hpp"
#include "libjazz/stereo_sample.hpp"
#include "mod.hpp"
#include "regions.hpp"

namespace fridge::sound {
namespace {
using jazz::audio::StereoSample;
};

struct Update {
  enum Kind { kErase, kWrite } kind;
  size_t finished_at;
  float value;
  // For erases only: precomputed (FADE_TIME * value) / (1 - value). Cached
  // here so Read doesn't recompute it every sample. Updated whenever `value`
  // changes (see BufferValue::PushBack). Unused for writes.
  float erase_frac_offset;
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

class IndicesToUpdate {
 private:
  size_t index_;
  IndicesToUpdate* next_ = nullptr;
  static Slab<IndicesToUpdate, kUpdateCap> SLAB;

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
    Update* erase_update;  // latest pending erase, or nullptr

    SampleWithUpdates(float sample = 0.0f, Update* first_update = nullptr)
        : sample(sample),
          first_update(first_update),
          last_update(first_update),
          erase_update(nullptr) {}
  };
  static Slab<SampleWithUpdates, kUpdateCap> SAMPLES;
  using SlabPtr = decltype(SAMPLES)::Ptr<&SAMPLES>;

  BufferValue();
  explicit BufferValue(float sample);
  explicit BufferValue(SlabPtr ptr);
  BufferValue(const BufferValue&) = delete;
  BufferValue& operator=(const BufferValue&) = delete;
  ~BufferValue();

  bool isSampleWithUpdates() const { return (bits_ & kUpdateTag) != 0; }
  bool isSample() const { return !isSampleWithUpdates(); }
  float sample();
  void setSample(float sample);

  Update* PushBack(Update&& update);

  Update** FirstUpdate();

  void Housekeep();

  /** Call before freeing a node that has been unlinked from the list */
  void OnUpdateFreed(Update* freed_update);

 private:
  // Nonnegative floats occupy the lower 31 bits. A tagged slab index fits
  // in the same 32-bit word on both the host and the Seed, without union UB.
  static constexpr uint32_t kUpdateTag = 1u << 31;
  uint32_t bits_ = std::bit_cast<uint32_t>(1.0f);

  SlabPtr asSampleWithUpdates() {
    assert(isSampleWithUpdates());
    return SlabPtr::FromInt(bits_ & ~kUpdateTag);
  }
  float asSample() const { return std::bit_cast<float>(bits_) - 1.0f; }
};

class Sound {
 private:
  static constexpr const size_t global_clock_max_ = SIZE_MAX - 1;
  size_t global_clock_ = 0;

  std::array<IndicesToUpdate*, kFadeTime> indices_to_update_{};
  std::array<BufferValue, kBufferLen> left_buffer_;
  std::array<BufferValue, kBufferLen> right_buffer_;
  regions::Memory regions_;

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
  StereoSample Read(size_t position);

  /**
   * Write a value `sample` to the buffer at `position`
   */
  void Write(size_t position, StereoSample sample);

  /**
   * Erase the value in the buffer at `position` by multiplying it by `amount`
   * (which should be between 0 and 1)
   * */
  void Erase(size_t position, StereoSample amount);

  StereoSample ApplyHead(const fridge::config::Head& head, StereoSample sample,
                         bool use_regions);

 public:
  /**
   * Process an audio sample. Moves the internal clock forwards by 1
   */
  StereoSample ProcessSample(const fridge::mod::Frame& frame,
                             StereoSample sample);
};

}  // namespace fridge::sound

#endif  // SOUND_H_
