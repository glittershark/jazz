#include "fridge.hpp"
#include "gtest/gtest.h"

using namespace fridge;

namespace {

TEST(UITest, UI_can_be_constructed) {
  ui::UI ui;
}

TEST(UITest, update_a_single_knob_and_generate_config) {
  const float read_increment = 0.5f;

  ui::UI ui;
  ASSERT_EQ(ui.head.read_amount, 0.0f);

  auto callback = ui.head.read_amount.GetCallback();
  callback(21307 /* garbage */, read_increment);
  config::Config config;
  ui.UpdateConfig(config);

  EXPECT_EQ(config.heads[0].read_amount, read_increment);
}

}  // namespace
