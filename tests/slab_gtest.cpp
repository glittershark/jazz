#include "libjazz/slab.hpp"
#include <cstdint>
#include <gtest/gtest.h>

class SlabTest : public ::testing::Test {
protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(SlabTest, IntSlab) {
  Slab<uint64_t, 10> slab;

  auto x = slab.Alloc();
  *x = 4;
  auto y = slab.Alloc();
  *y = 7;
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

// TODO: test constructor and destructor get called
