#include <atomic>
#include <cstdlib>
#include <memory>
#include <new>

#include "../../benchmarks/fridge/workload.hpp"
#include "gtest/gtest.h"

namespace {
std::atomic<bool> count_allocations = false;
std::atomic<size_t> allocations = 0;
}  // namespace

void* operator new(std::size_t size) {
  if (count_allocations.load(std::memory_order_relaxed)) {
    allocations.fetch_add(1, std::memory_order_relaxed);
  }
  if (void* memory = std::malloc(size == 0 ? 1 : size)) {
    return memory;
  }
  throw std::bad_alloc();
}
void* operator new[](std::size_t size) {
  return ::operator new(size);
}
void operator delete(void* memory) noexcept {
  std::free(memory);
}
void operator delete[](void* memory) noexcept {
  std::free(memory);
}
void operator delete(void* memory, std::size_t) noexcept {
  std::free(memory);
}
void operator delete[](void* memory, std::size_t) noexcept {
  std::free(memory);
}
void* operator new(std::size_t size, std::align_val_t alignment) {
  if (count_allocations.load(std::memory_order_relaxed)) {
    allocations.fetch_add(1, std::memory_order_relaxed);
  }
  void* memory = nullptr;
  if (posix_memalign(&memory, static_cast<size_t>(alignment),
                     size == 0 ? 1 : size) == 0) {
    return memory;
  }
  throw std::bad_alloc();
}
void* operator new[](std::size_t size, std::align_val_t alignment) {
  return ::operator new(size, alignment);
}
void operator delete(void* memory, std::align_val_t) noexcept {
  std::free(memory);
}
void operator delete[](void* memory, std::align_val_t) noexcept {
  std::free(memory);
}
void operator delete(void* memory, std::size_t, std::align_val_t) noexcept {
  std::free(memory);
}
void operator delete[](void* memory, std::size_t, std::align_val_t) noexcept {
  std::free(memory);
}

class FridgeRealtimeTest : public testing::TestWithParam<size_t> {};

TEST_P(FridgeRealtimeTest, CallbackAndLiveControlEditsAllocateNoHeapMemory) {
  auto sound = std::make_unique<fridge::sound::Sound>();
  fridge::benchmark::Workload workload(GetParam());
  allocations = 0;
  count_allocations = true;
  double checksum = 0;
  for (size_t sample = 0; sample < 12000; ++sample) {
    const auto output = workload.Tick(*sound);
    checksum += output.left + output.right;
  }
  count_allocations = false;
  EXPECT_EQ(allocations.load(), 0);
  EXPECT_TRUE(std::isfinite(checksum));
}

INSTANTIATE_TEST_SUITE_P(AllWorkloads, FridgeRealtimeTest,
                         testing::Range(size_t{0},
                                        fridge::benchmark::kScenarios.size()));
