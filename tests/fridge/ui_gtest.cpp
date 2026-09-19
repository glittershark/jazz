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
    for (size_t i = 0; i < kNumHeads; ++i) {
      initial_config.heads[i].write_amount = 0.0f;
      initial_config.heads[i].read_amount = 0.0f;
      initial_config.heads[i].erase_amount = 0.0f;
    }

    for (size_t i = 0; i < kNumLfos; ++i) {
      initial_config.lfos[i].max_grain_size = 0;
      initial_config.lfos[i].min_grain_size = 0;
    }
  }
};

TEST_F(UITest, UI_can_be_constructed) {
  config::ConfigStore store;
  ui::UI ui(leds, &store);
}

TEST_F(UITest, update_head_knobs_via_callbacks) {
  const unsigned garbage = 21307;
  const size_t selected_head = 1;

  config::ConfigStore store(initial_config);
  ui::UI ui(leds, &store);
  ui.SelectHead(selected_head);
  config::Config config = ui.Config();

  ui.head_knobs().position.GetCallback()(garbage, 0.25f);
  config = ui.Config();
  EXPECT_EQ(config.heads[selected_head].position, kSampleRateHz / 4);

  auto test_single_turn_knob = [&](ui::Knob<ui::SingleTurn>& knob,
                                   const float& target, float increment) {
    ASSERT_FLOAT_EQ(target, 0);

    auto callback = knob.GetCallback();
    callback(garbage, increment);
    config = ui.Config();

    EXPECT_FLOAT_EQ(target, increment);
  };

  test_single_turn_knob(ui.head_knobs().write_amount,
                        config.heads[selected_head].write_amount, 0.1f);
  test_single_turn_knob(ui.head_knobs().read_amount,
                        config.heads[selected_head].read_amount, 0.2f);
  test_single_turn_knob(ui.head_knobs().erase_amount,
                        config.heads[selected_head].erase_amount, 0.35f);

  {
    const float feedback_increment = -0.3f;

    ASSERT_EQ(ui.head_knobs().feedback.Get(), config::Feedback{});

    auto callback = ui.head_knobs().feedback.GetCallback();
    callback(garbage, feedback_increment);
    config = ui.Config();

    EXPECT_EQ(config.heads[selected_head].feedback.kind,
              config::Feedback::Kind::kErase);
    EXPECT_FLOAT_EQ(config.heads[selected_head].feedback.amount,
                    -feedback_increment);
  }
}

TEST_F(UITest, update_lfo_knobs_via_callbacks) {
  const unsigned garbage = 21307;
  const size_t selected_lfo = 1;

  config::ConfigStore store(initial_config);
  ui::UI ui(leds, &store);
  ui.SelectHead(selected_lfo);
  config::Config config = ui.Config();

  auto test_one_turn_is_buffer_len_knob =
      [&](ui::Knob<ui::OneTurnIsBufferLen>& knob, const size_t& target,
          float turn) {
        ASSERT_EQ(target, 0);

        auto callback = knob.GetCallback();
        callback(garbage, turn);
        config = ui.Config();

        EXPECT_EQ(target, turn * kBufferLen);
      };

  test_one_turn_is_buffer_len_knob(ui.lfo_knobs().max_grain_size,
                                   config.lfos[selected_lfo].max_grain_size,
                                   .17);
  test_one_turn_is_buffer_len_knob(ui.lfo_knobs().min_grain_size,
                                   config.lfos[selected_lfo].min_grain_size,
                                   .33);

  auto test_single_turn_knob = [&](ui::Knob<ui::SingleTurn>& knob,
                                   const float& target, float increment) {
    ASSERT_FLOAT_EQ(target, 0);

    auto callback = knob.GetCallback();
    callback(garbage, increment);
    config = ui.Config();

    EXPECT_FLOAT_EQ(target, increment);
  };

  test_single_turn_knob(ui.lfo_knobs().reverse_chance,
                        config.lfos[selected_lfo].reverse_chance, 0.7f);
  test_single_turn_knob(ui.lfo_knobs().teleport_chance,
                        config.lfos[selected_lfo].teleport_chance, 0.6f);
  test_single_turn_knob(ui.lfo_knobs().pitch_shift_chance,
                        config.lfos[selected_lfo].pitch_shift_chance, 0.5f);
  test_single_turn_knob(ui.lfo_knobs().low_octave_chance,
                        config.lfos[selected_lfo].low_octave_chance, 0.4f);
  test_single_turn_knob(ui.lfo_knobs().high_octave_chance,
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

TEST_F(UITest, one_turn_is_buffer_len_knob_saturates) {
  ui::Knob<ui::OneTurnIsBufferLen> knob("test knob");

  ASSERT_EQ(knob.Get(), 0);

  knob.GetCallback()(0, .25);
  EXPECT_EQ(knob.Get(), .25 * kBufferLen);

  knob.GetCallback()(0, -.5);
  EXPECT_EQ(knob.Get(), 0);

  knob.GetCallback()(0, 1.25);
  EXPECT_EQ(knob.Get(), kBufferLen);
}

TEST_F(UITest, pan_knob_converts_to_config_pan) {
  ui::Knob<ui::Pan> knob("test knob");

  EXPECT_TRUE(config::Pan(knob.Get()).IsCenter());

  knob.GetCallback()(0, 0.25f);
  EXPECT_FLOAT_EQ(config::Pan(knob.Get()).pan(), 0.5f);

  knob.GetCallback()(0, -0.5f);
  EXPECT_FLOAT_EQ(config::Pan(knob.Get()).pan(), -0.5f);

  knob.GetCallback()(0, -1.0f);
  EXPECT_FLOAT_EQ(config::Pan(knob.Get()).pan(), -1.0f);
}

TEST_F(UITest, pan_survives_a_head_switch) {
  config::ConfigStore store;
  ui::UI ui(leds, &store);

  ui.head_knobs().pan.GetCallback()(0, 0.25f);
  ASSERT_FLOAT_EQ(ui.Config().heads[0].pan.pan(), 0.5f);

  ui.SelectHead(1);
  ui.SelectHead(0);

  // Nudging the knob by nothing writes back whatever SelectHead loaded into it
  ui.head_knobs().pan.GetCallback()(0, 0.0f);
  EXPECT_FLOAT_EQ(ui.Config().heads[0].pan.pan(), 0.5f);
}

TEST_F(UITest, wet_knob_uses_led_to_display) {
  config::ConfigStore store;
  ui::UI ui(leds, &store);

  auto& knob = ui.wet_knob();

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

  config::ConfigStore store(initial_config);
  ui::UI ui(leds, &store);

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

  config::ConfigStore store(initial_config);
  ui::UI ui(leds, &store);

  auto new_config = ui.Config();

  EXPECT_FLOAT_EQ(new_config.dry, initial_config.dry);
  EXPECT_FLOAT_EQ(new_config.wet, initial_config.wet);
}

TEST_F(UITest, change_selected_head_via_radio_buttons) {
  config::ConfigStore store;
  ui::UI ui(leds, &store);

  ui.head_select().Select(2);
  EXPECT_EQ(ui.selected_head(), 2);

  EXPECT_EQ(ui.Config().heads[2].position, 0);

  ui.head_knobs().position.GetCallback()(0, 0.5);

  auto config = ui.Config();
  EXPECT_EQ(config.heads[2].position, 0.5 * kSampleRateHz);
}

TEST_F(UITest, head_button_selects_the_paired_lfo) {
  config::ConfigStore store;
  ui::UI ui(leds, &store);

  ui.head_select().Select(2);
  EXPECT_EQ(ui.selected_head(), 2);

  EXPECT_EQ(ui.Config().lfos[2].high_octave_chance, 0);

  ui.lfo_knobs().high_octave_chance.GetCallback()(0, 0.5f);

  auto config = ui.Config();
  EXPECT_FLOAT_EQ(config.lfos[2].high_octave_chance, 0.5f);
}

TEST_F(UITest, all_ten_pairs_and_six_regions_are_selectable) {
  config::ConfigStore store;
  ui::UI ui(leds, &store);
  for (uint8_t head = 0; head < kNumHeads; ++head) {
    ui.head_select().Select(head);
    EXPECT_EQ(ui.selected_head(), head);
    ui.lfo_knobs().reverse_chance.GetCallback()(0, 0.25f);
    EXPECT_FLOAT_EQ(store.Read().lfos[head].reverse_chance, 0.25f);
    for (uint8_t region = 0; region < kNumRegions; ++region) {
      ui.region_select().Select(region);
      EXPECT_EQ(store.Read().heads[head].region, region);
      EXPECT_EQ(ui.selected_head(), head);
    }
  }
}

TEST_F(UITest, selection_loads_region_without_reassigning_the_head) {
  initial_config.heads[9].region = 5;
  initial_config.regions[5].range = 2000;
  config::ConfigStore store(initial_config);
  ui::UI ui(leds, &store);
  ui.head_select().Select(9);
  EXPECT_EQ(ui.region_select().selected(), 5);
  EXPECT_EQ(ui.range_knob().Get(), 2000);
  EXPECT_EQ(store.Read(), initial_config);
}

TEST_F(UITest, position_wraps_within_region_and_assignment_preserves_fraction) {
  initial_config.regions[0].range = 1000;
  initial_config.regions[5].range = 2000;
  config::ConfigStore store(initial_config);
  ui::UI ui(leds, &store);
  ui.head_knobs().position.GetCallback()(0, -0.25f);
  EXPECT_EQ(store.Read().heads[0].position, 750);
  ui.region_select().Select(5);
  EXPECT_EQ(store.Read().heads[0].position, 1500);
  ui.head_knobs().position.GetCallback()(0, 0.5f);
  EXPECT_EQ(store.Read().heads[0].position, 500);
}

TEST_F(UITest, range_is_shared_and_does_not_edit_lfo_parameters) {
  initial_config.heads[9].position = kSampleRateHz / 2;
  config::ConfigStore store(initial_config);
  ui::UI ui(leds, &store);
  ui.range_knob().GetCallback()(0, 0.25f);
  const size_t range = kSampleRateHz + kBufferLen / 4;
  EXPECT_EQ(store.Read().regions[0].range, range);
  EXPECT_EQ(store.Read().heads[9].position, range / 2);
  EXPECT_EQ(store.Read().lfos, initial_config.lfos);
  ui.head_select().Select(9);
  EXPECT_EQ(ui.range_knob().Get(), range);
  EXPECT_FLOAT_EQ(ui.head_knobs().position.Get(), 0.5f);
}

TEST_F(UITest, rejected_range_is_unchanged_and_flashes_then_recovers) {
  for (auto& region : initial_config.regions) {
    region.range = kRegionPageSize;
  }
  initial_config.regions[0].range =
      kRegionCapacity - (kNumRegions - 1) * kRegionPageSize;
  config::ConfigStore store(initial_config);
  ui::UI ui(leds, &store);
  ui.range_knob().GetCallback()(0, 0.25f);
  EXPECT_EQ(store.Read(), initial_config);
  EXPECT_FALSE(store.dirty());
  EXPECT_EQ(ui.range_knob().Get(), initial_config.regions[0].range);
  ui.Tick(0);
  EXPECT_TRUE(ui.range_knob().rgb_led().red().on());
  EXPECT_EQ(ui.range_knob().rgb_led().red().duty(), 255);
  EXPECT_EQ(ui.range_knob().rgb_led().green().duty(), 0);
  ui.Tick(100);
  EXPECT_FALSE(ui.range_knob().rgb_led().red().on());
  ui.Tick(600);
  EXPECT_TRUE(ui.range_knob().rgb_led().red().on());
  ui.range_knob().GetCallback()(0, -0.25f);
  EXPECT_LT(store.Read().regions[0].range, initial_config.regions[0].range);
}

TEST_F(UITest, stored_generic_targets_do_not_decouple_selection_or_change) {
  initial_config.lfos[9].targets[0] = config::Target{
      .object = config::TargetObject::kMixer,
      .parameter = config::TargetParameter::kDry,
  };
  config::ConfigStore store(initial_config);
  ui::UI ui(leds, &store);
  ui.head_select().Select(9);
  ui.lfo_knobs().reverse_chance.GetCallback()(0, 0.5f);
  EXPECT_FLOAT_EQ(store.Read().lfos[9].reverse_chance, 0.5f);
  EXPECT_EQ(store.Read().lfos[9].targets, initial_config.lfos[9].targets);
}

}  // namespace
