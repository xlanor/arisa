#include <arisa/provider/input/buttons.hpp>
#include <arisa/provider/input/input_tuning.hpp>

#include <gtest/gtest.h>

#include <cmath>

namespace arisa::provider {

TEST(ButtonsTest, AddsRemovesAndMerges) {
  constexpr Buttons pressed{Button::a, Button::zl};
  static_assert(pressed.contains(Button::a));
  EXPECT_TRUE(pressed.contains(Button::zl));
  EXPECT_FALSE(pressed.contains(Button::b));
  EXPECT_EQ(pressed.with(Button::b).without(Button::a),
            (Buttons{Button::b, Button::zl}));
  EXPECT_EQ(pressed | Buttons{Button::plus},
            (Buttons{Button::a, Button::zl, Button::plus}));
  EXPECT_TRUE(Buttons{}.empty());
  EXPECT_EQ(all_buttons.size(), 18U);
}

TEST(InputTuningTest, DefaultsAndRejectsOutOfRangeDeadzones) {
  EXPECT_FLOAT_EQ(InputTuning{}.deadzone(), InputTuning::default_deadzone);
  EXPECT_FALSE(InputTuning::create(-0.1F).has_value());
  EXPECT_FALSE(InputTuning::create(1.0F).has_value());
  EXPECT_TRUE(InputTuning::create(0.0F).has_value());
}

TEST(InputTuningTest, AppliesARadialDeadzoneAndRescales) {
  const auto tuning = InputTuning::create(0.2F);
  ASSERT_TRUE(tuning.has_value());
  EXPECT_EQ(tuning->apply(Stick{.x = 0.1F, .y = 0.1F}), Stick{});
  const Stick full = tuning->apply(Stick{.x = 1.0F, .y = 0.0F});
  EXPECT_FLOAT_EQ(full.x, 1.0F);
  EXPECT_FLOAT_EQ(full.y, 0.0F);
  const Stick half = tuning->apply(Stick{.x = 0.0F, .y = -0.6F});
  EXPECT_NEAR(half.y, -0.5F, 1.0e-5F);
  const Stick corner = tuning->apply(Stick{.x = 1.0F, .y = 1.0F});
  EXPECT_NEAR(std::hypot(corner.x, corner.y), 1.0F, 1.0e-5F);
}

}
