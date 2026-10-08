#include "callback.hpp"
#include "gtest/gtest.h"

TEST(CallbackTest, EmptyCallbacksCanBeInvoked) {
  const Callback<int> callback;
  EXPECT_FALSE(static_cast<bool>(callback));
  callback(7);
}

TEST(CallbackTest, CopiesInvokeTheSameContextWithTheCorrectFunctionSignature) {
  struct Receiver {
    int total = 0;
    float gain = 0;
  } receiver;
  const Callback<int, float> callback{
      .callback =
          +[](void* context, int increment, float gain) {
            auto& receiver = *static_cast<Receiver*>(context);
            receiver.total += increment;
            receiver.gain = gain;
          },
      .data = &receiver};
  const auto copy = callback;
  callback(3, 0.5f);
  copy(4, 0.25f);
  EXPECT_TRUE(static_cast<bool>(copy));
  EXPECT_EQ(receiver.total, 7);
  EXPECT_FLOAT_EQ(receiver.gain, 0.25f);
}
