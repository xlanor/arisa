#include <arisa/composition/move_command_direction.hpp>
#include <arisa/provider/input/input_tuning.hpp>
#include <arisa/provider/input/navigation_tuning.hpp>
#include <arisa/provider/input/stick.hpp>

#include <interaction/stick_navigation.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <optional>

namespace arisa::interaction {

using composition::MoveCommandDirection;
using provider::Stick;

TEST(StickNavigationTest, DeadzoneRemovesRestingNoise) {
  const provider::InputTuning tuning;
  EXPECT_EQ(tuning.apply(Stick{.x = 0.1F, .y = -0.05F}), Stick{});
  const Stick pushed = tuning.apply(Stick{.x = 1.0F, .y = 0.0F});
  EXPECT_FLOAT_EQ(pushed.x, 1.0F);
}

TEST(StickNavigationTest, EngagesOnlyPastTheThreshold) {
  StickNavigation stick;
  const provider::NavigationTuning tuning;
  EXPECT_FALSE(stick.update(Stick{.x = 0.4F, .y = 0.0F}, tuning).has_value());
  EXPECT_EQ(stick.update(Stick{.x = 0.6F, .y = 0.0F}, tuning),
            MoveCommandDirection::right);
  EXPECT_EQ(stick.update(Stick{.x = 0.0F, .y = -0.7F}, tuning),
            MoveCommandDirection::down);
}

TEST(StickNavigationTest, DiagonalNoiseKeepsTheHeldDirection) {
  StickNavigation stick;
  const provider::NavigationTuning tuning;
  EXPECT_EQ(stick.update(Stick{.x = 0.8F, .y = 0.1F}, tuning),
            MoveCommandDirection::right);
  EXPECT_EQ(stick.update(Stick{.x = 0.6F, .y = 0.65F}, tuning),
            MoveCommandDirection::right);
  EXPECT_EQ(stick.update(Stick{.x = 0.62F, .y = 0.6F}, tuning),
            MoveCommandDirection::right);
  EXPECT_EQ(stick.update(Stick{.x = 0.3F, .y = 0.8F}, tuning),
            MoveCommandDirection::up);
}

TEST(StickNavigationTest, ReleasesBelowTheReleaseThreshold) {
  StickNavigation stick;
  const provider::NavigationTuning tuning;
  static_cast<void>(stick.update(Stick{.x = -0.9F, .y = 0.0F}, tuning));
  EXPECT_EQ(stick.update(Stick{.x = -0.4F, .y = 0.0F}, tuning),
            MoveCommandDirection::left);
  EXPECT_FALSE(stick.update(Stick{.x = -0.2F, .y = 0.0F}, tuning).has_value());
}

TEST(StickNavigationTest, ReversalOnTheSameAxisSwitchesImmediately) {
  StickNavigation stick;
  const provider::NavigationTuning tuning;
  static_cast<void>(stick.update(Stick{.x = 0.9F, .y = 0.0F}, tuning));
  EXPECT_EQ(stick.update(Stick{.x = -0.9F, .y = 0.0F}, tuning),
            MoveCommandDirection::left);
}

TEST(StickNavigationTest, TuningRejectsInconsistentNavigation) {
  provider::NavigationTuning inverted;
  inverted.stick_release = 0.8F;
  EXPECT_FALSE(provider::InputTuning::create(0.1F, inverted).has_value());
  provider::NavigationTuning stalled;
  stalled.repeat_interval = std::chrono::milliseconds{0};
  EXPECT_FALSE(provider::InputTuning::create(0.1F, stalled).has_value());
  provider::NavigationTuning loose;
  loose.direction_hysteresis = 0.5F;
  EXPECT_FALSE(provider::InputTuning::create(0.1F, loose).has_value());
  EXPECT_TRUE(provider::InputTuning::create(0.1F, provider::NavigationTuning{})
                  .has_value());
}

}
