#include "libjazz/slab.hpp"
#include <atomic>
#include <cstdint>
#include <gtest/gtest.h>

class SlabTest : public ::testing::Test {
protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(SlabTest, IntSlab) {
  Slab<uint64_t, 10> slab;

  auto x = slab.Alloc(4);
  auto y = slab.Alloc(7);
  EXPECT_EQ(*x, 4);
  EXPECT_EQ(*y, 7);

  slab.Free(y);
  EXPECT_EQ(*x, 4);

  auto z = slab.Alloc();
  EXPECT_EQ(y, z);

  slab.Free(z);
  slab.Free(y);
  slab.Free(x);

  auto x_new = slab.Alloc();
  auto y_new = slab.Alloc();
  auto z_new = slab.Alloc();

  EXPECT_EQ(z_new, z);
  EXPECT_EQ(y_new, y);
  EXPECT_EQ(x_new, x);
}

class CstrDestr {
  std::atomic<int> *m_cstr_calls;
  std::atomic<int> *m_destr_calls;

public:
  CstrDestr(std::atomic<int> *cstr_calls, std::atomic<int> *destr_calls)
      : m_cstr_calls(cstr_calls), m_destr_calls(destr_calls) {
    (*m_cstr_calls)++;
  }

  ~CstrDestr() { (*m_destr_calls)++; }
};

TEST_F(SlabTest, CstrDestr) {
  Slab<CstrDestr, 10> slab;

  std::atomic<int> cstr_calls{0};
  std::atomic<int> destr_calls{0};

  auto x = slab.Alloc(&cstr_calls, &destr_calls);
  EXPECT_EQ(cstr_calls, 1);

  auto y = slab.Alloc(&cstr_calls, &destr_calls);
  EXPECT_NE(x, y);
  EXPECT_EQ(cstr_calls, 2);

  slab.Free(x);
  EXPECT_EQ(destr_calls, 1);

  slab.Free(y);
  EXPECT_EQ(destr_calls, 2);
}

TEST_F(SlabTest, CstrDestrUniquePtr) {
  Slab<CstrDestr, 10> slab;

  std::atomic<int> cstr_calls{0};
  std::atomic<int> destr_calls{0};

  void *orig_x;
  {
    auto x = slab.MakeUnique(&cstr_calls, &destr_calls);
    orig_x = x.get();
    EXPECT_EQ(cstr_calls, 1);
  }
  EXPECT_EQ(destr_calls, 1);

  {
    auto x = slab.MakeUnique(&cstr_calls, &destr_calls);
    EXPECT_EQ(cstr_calls, 2);
    EXPECT_EQ(x.get(), orig_x);
  }
  EXPECT_EQ(destr_calls, 2);
}
