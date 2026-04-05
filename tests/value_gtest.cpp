#include <gtest/gtest.h>

#include "libjazz/value.hpp"

class ValueTest : public ::testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(ValueTest, FloatConstructorAndGetter) {
  Value v(3.14f);
  EXPECT_TRUE(v.isFloat());
  EXPECT_FALSE(v.isPointer());

  // Use appropriate epsilon for the platform's floating point type
#if UINTPTR_MAX == 0xFFFFFFFF
  EXPECT_FLOAT_EQ(3.14f, v.getFloat());
#else
  // On 64-bit platforms, float gets converted to double through constructor
  EXPECT_DOUBLE_EQ(static_cast<double>(3.14f), v.getFloat());
#endif
}

TEST_F(ValueTest, PointerConstructorAndGetter) {
  // Use a smaller address that won't conflict with tag bits
  void* test_ptr = reinterpret_cast<void*>(0x1000);
  Value v(test_ptr);
  EXPECT_FALSE(v.isFloat());
  EXPECT_TRUE(v.isPointer());
  EXPECT_EQ(test_ptr, v.getPointer());
}

TEST_F(ValueTest, NegativeZeroStaysAsFloat) {
  Value v(-0.0f);
  EXPECT_TRUE(v.isFloat());
}

TEST_F(ValueTest, LargeFloatStaysAsFloat) {
  Value v(100000.5f);
  EXPECT_TRUE(v.isFloat());

#if UINTPTR_MAX == 0xFFFFFFFF
  EXPECT_FLOAT_EQ(100000.5f, v.getFloat());
#else
  EXPECT_DOUBLE_EQ(static_cast<double>(100000.5f), v.getFloat());
#endif
}

TEST_F(ValueTest, DoubleConstructor) {
  Value v(3.141592653589793);
  EXPECT_TRUE(v.isFloat());
  EXPECT_FALSE(v.isPointer());

#if UINTPTR_MAX == 0xFFFFFFFFFFFFFFFF
  // On 64-bit platforms, we should preserve double precision
  EXPECT_DOUBLE_EQ(3.141592653589793, v.getFloat());
#else
  // On 32-bit platforms, we convert to float
  EXPECT_FLOAT_EQ(static_cast<float>(3.141592653589793), v.getFloat());
#endif
}
