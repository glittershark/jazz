#include "config.hpp"
#include "gtest/gtest.h"
#include "led.hpp"
#include "ui.hpp"

using namespace fridge;

namespace {

struct UITest : public testing::Test {
  io::led::Controller leds;
};

TEST_F(UITest, UI_can_be_constructed) {
  ui::UI ui(leds);
}

TEST_F(UITest, update_head_knobs_via_callbacks) {
  const unsigned garbage = 21307;
  const size_t selected_head = 1;

  ui::UI ui(leds);
  ui.selected_head = selected_head;
  config::Config config;

  auto test_position_knob = [&](ui::Knob<ui::Position>& knob,
                                const size_t& target, size_t increment) {
    ASSERT_EQ(target, 0);

    auto callback = knob.GetCallback();
    callback(increment, garbage);
    ui.UpdateConfig(config);

    EXPECT_EQ(target, increment);
  };

  test_position_knob(ui.head.position, config.heads[selected_head].position,
                     47);

  auto test_single_turn_knob = [&](ui::Knob<ui::SingleTurn>& knob,
                                   const float& target, float increment) {
    ASSERT_FLOAT_EQ(target, 0);

    auto callback = knob.GetCallback();
    callback(garbage, increment);
    ui.UpdateConfig(config);

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
    ui.UpdateConfig(config);

    EXPECT_EQ(config.heads[selected_head].feedback.kind,
              config::Feedback::Kind::kErase);
    EXPECT_FLOAT_EQ(config.heads[selected_head].feedback.amount,
                    -feedback_increment);
  }
}

TEST_F(UITest, update_lfo_knobs_via_callbacks) {
  const unsigned garbage = 21307;
  const size_t selected_lfo = 1;

  ui::UI ui(leds);
  ui.selected_lfo = selected_lfo;
  config::Config config;

  auto test_size_knob = [&](ui::Knob<ui::Size>& knob, const size_t& target,
                            size_t increment) {
    ASSERT_EQ(target, 0);

    auto callback = knob.GetCallback();
    callback(increment, garbage);
    ui.UpdateConfig(config);

    EXPECT_EQ(target, increment);
  };

  auto test_position_knob = [&](ui::Knob<ui::Position>& knob,
                                const size_t& target, size_t increment) {
    ASSERT_EQ(target, 0);

    auto callback = knob.GetCallback();
    callback(increment, garbage);
    ui.UpdateConfig(config);

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
    ui.UpdateConfig(config);

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
  ui::Knob<ui::SingleTurn> knob;
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
  ui::Knob<ui::Position> knob;

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

}  // namespace
