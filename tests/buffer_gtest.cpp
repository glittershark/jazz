#include <gtest/gtest.h>

#include <algorithm>

#include "libjazz/buffer.hpp"

using namespace jazz::buffer;

TEST(CircularArrayTest, correct_indexing) {
  CircularArray<int, 993> buf;

  EXPECT_EQ(buf.real_index_(0), 0);
  EXPECT_EQ(buf.real_index_(1), 1);

  EXPECT_EQ(buf.real_index_(buf.size()), 0);
  EXPECT_EQ(buf.real_index_(buf.size() + 1), 1);

  EXPECT_EQ(buf.real_index_(-1), buf.size() - 1);
  EXPECT_EQ(buf.real_index_(-buf.size()), 0);
}

TEST(CircularArrayTest, initializes_to_zero) {
  CircularArray<int, 1024> buf;
  for (int i = 0; i < buf.size(); ++i) {
    EXPECT_EQ(buf[i], 0);
  }
}

TEST(CircularArrayTest, PushBack_pushes_onto_the_end) {
  CircularArray<int, 33> buf;

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

TEST(CircularArrayTest, PushBack_with_negative_indices) {
  CircularArray<int, 47> buf;

  for (int i = 0; i < buf.size(); ++i) {
    buf.PushBack(i);
  }

  for (int i = 0; i < buf.size(); ++i) {
    ptrdiff_t diff = i - buf.size();
    EXPECT_EQ(buf[diff], i);
  }

  // for clarity: buf[-1] refers to the thing we just wrote
  EXPECT_EQ(buf[-1], buf.size() - 1);
}

TEST(CircularArrayTest, Push_and_PushBack_in_tandem) {
  CircularArray<int, 93> buf;

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

TEST(CircularArrayTest, can_iterate_over_it) {
  CircularArray<int, 33> buf;
  int i = 0;

  for (auto& elem : buf) {
    elem = i++;
    std::cout << elem << '\n';
  }

  // check with conventional loop
  for (int i = 0; i < buf.size(); ++i) {
    EXPECT_EQ(buf[i], i);
  }
}

TEST(CircularArrayTest, can_sort_it) {
  CircularArray<int, 47> buf;
  int i = 0;

  for (std::size_t i = 0; i < buf.size(); ++i) {
    buf[i] = buf.size() - 1 - i;
  }

  std::sort(buf.begin(), buf.end());

  for (std::size_t i = 0; i < buf.size(); ++i) {
    EXPECT_EQ(buf[i], i);
  }
}
