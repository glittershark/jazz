#include <gtest/gtest.h>

#include "libjazz/buffer.hpp"

using namespace jazz::buffer;

TEST(LoopTest, correct_indexing) {
  Loop<int, 993> buf;

  EXPECT_EQ(buf.real_index_(0), 0);
  EXPECT_EQ(buf.real_index_(1), 1);

  EXPECT_EQ(buf.real_index_(buf.size()), 0);
  EXPECT_EQ(buf.real_index_(buf.size() + 1), 1);

  EXPECT_EQ(buf.real_index_(-1), buf.size() - 1);
  EXPECT_EQ(buf.real_index_(-buf.size()), 0);
}

TEST(LoopTest, initializes_to_zero) {
  Loop<int, 1024> buf;
  for (int i = 0; i < buf.size(); ++i) {
    EXPECT_EQ(buf[i], 0);
  }
}

TEST(LoopTest, PushBack_pushes_onto_the_end) {
  Loop<int, 33> buf;

  for (int i = 0; i < buf.size(); ++i) {
    EXPECT_EQ(buf.zero_, i);
    EXPECT_EQ(buf.PushBack(i), 0);
  }

  EXPECT_EQ(buf.zero_, 0);

  for (int i = 0; i < buf.size(); ++i) {
    EXPECT_EQ(buf[i], i);
  }

  EXPECT_EQ(buf.PushBack(9), 0);
  EXPECT_EQ(buf[0], 1);
}

TEST(LoopTest, PushBack_with_negative_indices) {
  Loop<int, 47> buf;

  for (int i = 0; i < buf.size(); ++i) {
    buf.PushBack(i);
  }

  for (int i = 0; i < buf.size(); ++i) {
    ptrdiff_t diff = i - buf.size();
    std::cout << diff << std::endl;
    EXPECT_EQ(buf[diff], i);
  }

  // for clarity: buf[-1] refers to the thing we just wrote
  EXPECT_EQ(buf[-1], buf.size() - 1);
}

TEST(LoopTest, Push_and_PushBack_in_tandem) {
  Loop<int, 93> buf;

  for (int i = 0; i < buf.size(); ++i) {
    buf[i] = i;
  }

  // pushing pops off the end of the buffer
  EXPECT_EQ(buf.Push(-1), buf.size() - 1);
  for (int i = 0; i < buf.size(); ++i) {
    EXPECT_EQ(buf[i], i - 1);
  }

  // pushing back pops off the beginning of the buffer
  EXPECT_EQ(-1, buf.PushBack(buf.size() - 1));
  EXPECT_EQ(0, buf.PushBack(buf.size()));
  for (int i = 0; i < buf.size(); ++i) {
    EXPECT_EQ(buf[i], i + 1);
  }
}
