#include "config.hpp"
#include "constants.hpp"
#include "gtest/gtest.h"
#include "led.hpp"
#include "ui.hpp"

using namespace fridge;

namespace {
using std::move;

struct UITest : public testing::Test {
  io::led::Controller leds;
  config::Config initial_config;

  UITest() : leds(), initial_config{} {
    for (size_t i = 0; i < NUM_HEADS; ++i) {
      initial_config.heads[i].write_amount = 0.0f;
      initial_config.heads[i].read_amount = 0.0f;
      initial_config.heads[i].erase_amount = 0.0f;
    }

    for (size_t i = 0; i < NUM_LFOS; ++i) {
      initial_config.lfos[i].max_grain_size = 0;
      initial_config.lfos[i].min_grain_size = 0;
    }
  }
};

TEST_F(UITest, UI_can_be_constructed) {
  ui::UI ui(leds);
}

TEST_F(UITest, update_head_knobs_via_callbacks) {
  const unsigned garbage = 21307;
  const size_t selected_head = 1;

  ui::UI ui(leds, initial_config);
  ui.selected_head = selected_head;
  config::Config config;

  auto test_position_knob = [&](ui::Knob<ui::Position>& knob,
                                const size_t& target, size_t increment) {
    ASSERT_EQ(target, 0);

    auto callback = knob.GetCallback();
    callback(increment, garbage);
    config = ui.Config();

    EXPECT_EQ(target, increment);
  };

  test_position_knob(ui.head.position, config.heads[selected_head].position,
                     47);

  auto test_single_turn_knob = [&](ui::Knob<ui::SingleTurn>& knob,
                                   const float& target, float increment) {
    ASSERT_FLOAT_EQ(target, 0);

    auto callback = knob.GetCallback();
    callback(garbage, increment);
    config = ui.Config();

    EXPECT_FLOAT_EQ(target, increment);
  };

  test_single_turn_knob(ui.head.write_amount,
                        config.heads[selected_head].write_amount, 0.1f);
  test_single_turn_knob(ui.head.read_amount,
                        config.heads[selected_head].read_amount, 0.2f);
  test_single_turn_knob(ui.head.erase_amount,
                        config.heads[selected_head].erase_amount, 0.35f);

  {
    const float feedback_increment = -0.3f;

    ASSERT_EQ(ui.head.feedback.Get(), config::Feedback{});

    auto callback = ui.head.feedback.GetCallback();
    callback(garbage, feedback_increment);
    config = ui.Config();

    EXPECT_EQ(config.heads[selected_head].feedback.kind,
              config::Feedback::Kind::kWrite);
    EXPECT_FLOAT_EQ(config.heads[selected_head].feedback.amount,
                    -feedback_increment);
  }
}

TEST_F(UITest, update_lfo_knobs_via_callbacks) {
  const unsigned garbage = 21307;
  const size_t selected_lfo = 1;

  ui::UI ui(leds, initial_config);
  ui.selected_lfo = selected_lfo;
  config::Config config;

  auto test_size_knob = [&](ui::Knob<ui::Size>& knob, const size_t& target,
                            size_t increment) {
    ASSERT_EQ(target, 0);

    auto callback = knob.GetCallback();
    callback(increment, garbage);
    config = ui.Config();

    EXPECT_EQ(target, increment);
  };

  auto test_position_knob = [&](ui::Knob<ui::Position>& knob,
                                const size_t& target, size_t increment) {
    ASSERT_EQ(target, 0);

    auto callback = knob.GetCallback();
    callback(increment, garbage);
    config = ui.Config();

    EXPECT_EQ(target, increment);
  };

  test_position_knob(ui.lfo.range, config.lfos[selected_lfo].range, 9);
  test_size_knob(ui.lfo.max_grain_size,
                 config.lfos[selected_lfo].max_grain_size, 17);
  test_size_knob(ui.lfo.min_grain_size,
                 config.lfos[selected_lfo].min_grain_size, 33);

  auto test_single_turn_knob = [&](ui::Knob<ui::SingleTurn>& knob,
                                   const float& target, float increment) {
    ASSERT_FLOAT_EQ(target, 0);

    auto callback = knob.GetCallback();
    callback(garbage, increment);
    config = ui.Config();

    EXPECT_FLOAT_EQ(target, increment);
  };

  test_single_turn_knob(ui.lfo.reverse_chance,
                        config.lfos[selected_lfo].reverse_chance, 0.7f);
  test_single_turn_knob(ui.lfo.teleport_chance,
                        config.lfos[selected_lfo].teleport_chance, 0.6f);
  test_single_turn_knob(ui.lfo.pitch_shift_chance,
                        config.lfos[selected_lfo].pitch_shift_chance, 0.5f);
  test_single_turn_knob(ui.lfo.low_octave_chance,
                        config.lfos[selected_lfo].low_octave_chance, 0.4f);
  test_single_turn_knob(ui.lfo.high_octave_chance,
                        config.lfos[selected_lfo].high_octave_chance, 0.3f);
}

TEST_F(UITest, single_turn_knob_saturates) {
  ui::Knob<ui::SingleTurn> knob("test knob");
  ASSERT_FLOAT_EQ(knob.Get(), 0.0f);

  knob.GetCallback()(0, 0.3f);
  EXPECT_FLOAT_EQ(knob.Get(), 0.3f);

  knob.GetCallback()(0, 0.3f);
  EXPECT_FLOAT_EQ(knob.Get(), 0.6f);

  knob.GetCallback()(0, 0.9f);
  EXPECT_FLOAT_EQ(knob.Get(), 1.0f);

  knob.GetCallback()(0, -0.5f);
  EXPECT_FLOAT_EQ(knob.Get(), 0.5f);

  knob.GetCallback()(0, -2.3f);
  EXPECT_FLOAT_EQ(knob.Get(), 0.0f);
}

TEST_F(UITest, position_knob_saturates) {
  const int third_of_buffer = static_cast<int>(BUFFER_LEN) / 3;
  ui::Knob<ui::Position> knob("test knob");

  ASSERT_EQ(knob.Get(), 0);

  knob.GetCallback()(third_of_buffer, 0);
  EXPECT_EQ(knob.Get(), third_of_buffer);

  knob.GetCallback()(-2 * third_of_buffer, 0);
  EXPECT_EQ(knob.Get(), 0);

  knob.GetCallback()(4 * third_of_buffer, 0);
  EXPECT_EQ(knob.Get(), BUFFER_LEN);
}

TEST_F(UITest, wet_knob_uses_led_to_display) {
  ui::UI ui(leds);

  auto& knob = ui.wet;

  knob.Set(0.7);

  auto rgb_led = knob.rgb_led();
  auto red = rgb_led.red();
  auto green = rgb_led.green();
  auto blue = rgb_led.blue();

  EXPECT_TRUE(red.on());
  EXPECT_TRUE(green.on());
  EXPECT_TRUE(blue.on());

  EXPECT_GT(green.duty(), 0);
  EXPECT_GT(blue.duty(), 0);
}

TEST_F(UITest, initial_config_is_left_unchanged_if_no_callbacks) {
  config::Config initial_config;
  initial_config.lfos[0].range = 44100;
  initial_config.lfos[0].max_grain_size = 19000;
  initial_config.lfos[0].min_grain_size = 2000;

  ui::UI ui(leds, initial_config);

  auto new_config = ui.Config();
  EXPECT_EQ(new_config.lfos[0].range, initial_config.lfos[0].range);
  EXPECT_EQ(new_config.lfos[0].max_grain_size,
            initial_config.lfos[0].max_grain_size);
  EXPECT_EQ(new_config.lfos[0].min_grain_size,
            initial_config.lfos[0].min_grain_size);
}

TEST_F(UITest, dry_and_wet_come_from_initial_config) {
  config::Config initial_config;
  initial_config.dry = 0.9;
  initial_config.wet = 0.777;

  ui::UI ui(leds, initial_config);

  auto new_config = ui.Config();

  EXPECT_FLOAT_EQ(new_config.dry, initial_config.dry);
  EXPECT_FLOAT_EQ(new_config.wet, initial_config.wet);
}

}  // namespace
