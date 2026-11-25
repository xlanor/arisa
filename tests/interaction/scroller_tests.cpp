#include <arisa/composition/axes.hpp>
#include <arisa/composition/point.hpp>
#include <arisa/composition/scroll_phase.hpp>
#include <arisa/composition/scroll_target_behavior.hpp>
#include <arisa/composition/size.hpp>
#include <arisa/motion/animation.hpp>

#include <interaction/scroller.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>

namespace arisa::interaction {

using composition::Point;
using composition::Size;
using std::chrono::milliseconds;
using std::chrono::nanoseconds;

class ScrollerTest : public ::testing::Test {
 protected:
  static auto metrics(composition::ScrollTargetBehavior behavior =
                          composition::ScrollTargetBehavior::standard)
      -> Scroller::Metrics {
    return Scroller::Metrics{.viewport = Size{.width = 300, .height = 400},
                             .content = Size{.width = 300, .height = 2000},
                             .axes = composition::Axes::vertical,
                             .behavior = behavior,
                             .targets = {}};
  }

  static void settle(Scroller& scroller, milliseconds from,
                     milliseconds limit = milliseconds{4000}) {
    for (milliseconds now = from; now <= from + limit && !scroller.idle();
         now += milliseconds{16}) {
      scroller.advance(now);
    }
  }
};

TEST_F(ScrollerTest, RubberBandFollowsTheScrollViewFormula) {
  EXPECT_DOUBLE_EQ(Scroller::rubber_band(0.0, 400.0), 0.0);
  const double expected =
      (1.0 - (1.0 / ((100.0 * 0.55 / 400.0) + 1.0))) * 400.0;
  EXPECT_DOUBLE_EQ(Scroller::rubber_band(100.0, 400.0), expected);
  EXPECT_LT(Scroller::rubber_band(100.0, 400.0), 100.0);
  EXPECT_LT(Scroller::rubber_band(100000.0, 400.0), 400.0);
}

TEST_F(ScrollerTest, DecayMathMatchesTheClosedForm) {
  const double rate = Scroller::normal_rate;
  EXPECT_NEAR(Scroller::decay_velocity(1000.0, rate, 1.0),
              1000.0 * std::pow(rate, 1000.0), 1e-9);
  EXPECT_NEAR(Scroller::decay_displacement(1000.0, rate, 0.0), 0.0, 1e-9);
  EXPECT_NEAR(Scroller::projection(1000.0, rate),
              -1000.0 / (1000.0 * std::log(rate)), 1e-9);
  EXPECT_NEAR(Scroller::decay_displacement(1000.0, rate, 60.0),
              Scroller::projection(1000.0, rate), 1e-3);
  EXPECT_GT(Scroller::projection(1000.0, rate),
            Scroller::projection(1000.0, Scroller::fast_rate));
}

TEST_F(ScrollerTest, DragFollowsTheFingerAndBandsPastTheEdge) {
  Scroller scroller;
  scroller.configure(metrics());
  scroller.grab(milliseconds{0});
  EXPECT_TRUE(scroller.dragging());
  scroller.drag(Size{.width = 0, .height = -150}, milliseconds{16});
  EXPECT_FLOAT_EQ(scroller.offset().y, 150.0F);
  scroller.drag(Size{.width = 0, .height = 100}, milliseconds{32});
  const auto banded = static_cast<float>(-Scroller::rubber_band(100.0, 400.0));
  EXPECT_FLOAT_EQ(scroller.offset().y, banded);
  EXPECT_EQ(scroller.phase(milliseconds{32}),
            composition::ScrollPhase::interacting);
}

TEST_F(ScrollerTest, ReleaseOutsideSpringsBack) {
  Scroller scroller;
  scroller.configure(metrics());
  scroller.grab(milliseconds{0});
  scroller.drag(Size{.width = 0, .height = 120}, milliseconds{16});
  scroller.release(Size{}, milliseconds{16});
  EXPECT_TRUE(scroller.bouncing());
  EXPECT_FLOAT_EQ(scroller.target().y, 0.0F);
  scroller.advance(milliseconds{116});
  EXPECT_LT(scroller.offset().y, 0.0F);
  EXPECT_GT(scroller.offset().y,
            static_cast<float>(-Scroller::rubber_band(120.0, 400.0)));
  settle(scroller, milliseconds{132});
  EXPECT_TRUE(scroller.idle());
  EXPECT_FLOAT_EQ(scroller.offset().y, 0.0F);
}

TEST_F(ScrollerTest, MomentumDecaysAlongTheClosedForm) {
  Scroller scroller;
  scroller.configure(metrics());
  scroller.grab(milliseconds{0});
  scroller.drag(Size{.width = 0, .height = -100}, milliseconds{16});
  scroller.release(Size{.width = 0, .height = -1000}, milliseconds{16});
  EXPECT_TRUE(scroller.decelerating());
  EXPECT_EQ(scroller.phase(milliseconds{16}),
            composition::ScrollPhase::decelerating);
  scroller.advance(milliseconds{216});
  const double expected =
      100.0 + Scroller::decay_displacement(1000.0, Scroller::normal_rate, 0.2);
  EXPECT_NEAR(scroller.offset().y, expected, 0.01);
  EXPECT_NEAR(scroller.velocity().height,
              Scroller::decay_velocity(1000.0, Scroller::normal_rate, 0.2),
              0.01);
  settle(scroller, milliseconds{232}, milliseconds{8000});
  EXPECT_TRUE(scroller.idle());
  EXPECT_NEAR(scroller.offset().y,
              100.0 + Scroller::projection(1000.0, Scroller::normal_rate), 5.0);
}

TEST_F(ScrollerTest, MomentumPastTheEndReboundsToTheLimit) {
  Scroller scroller;
  scroller.configure(metrics());
  scroller.grab(milliseconds{0});
  scroller.drag(Size{.width = 0, .height = -1500}, milliseconds{16});
  scroller.release(Size{.width = 0, .height = -3000}, milliseconds{16});
  settle(scroller, milliseconds{32}, milliseconds{8000});
  EXPECT_TRUE(scroller.idle());
  EXPECT_FLOAT_EQ(scroller.offset().y, 1600.0F);
}

TEST_F(ScrollerTest, PagingSnapsToTheNextPageWithinOne) {
  Scroller scroller;
  scroller.configure(metrics(composition::ScrollTargetBehavior::paging));
  scroller.grab(milliseconds{0});
  scroller.drag(Size{.width = 0, .height = -60}, milliseconds{16});
  scroller.release(Size{.width = 0, .height = -900}, milliseconds{16});
  EXPECT_TRUE(scroller.animating());
  EXPECT_FLOAT_EQ(scroller.target().y, 400.0F);
  settle(scroller, milliseconds{32});
  EXPECT_FLOAT_EQ(scroller.offset().y, 400.0F);
  scroller.grab(milliseconds{5000});
  scroller.drag(Size{.width = 0, .height = -20}, milliseconds{5016});
  scroller.release(Size{.width = 0, .height = -20000}, milliseconds{5016});
  EXPECT_FLOAT_EQ(scroller.target().y, 800.0F);
}

TEST_F(ScrollerTest, ViewAlignedSnapsToTheNearestTargetInTheFlingDirection) {
  Scroller scroller;
  Scroller::Metrics aligned =
      metrics(composition::ScrollTargetBehavior::view_aligned);
  aligned.targets = {0.0F, 250.0F, 500.0F, 750.0F};
  scroller.configure(aligned);
  scroller.grab(milliseconds{0});
  scroller.drag(Size{.width = 0, .height = -110}, milliseconds{16});
  scroller.release(Size{}, milliseconds{16});
  EXPECT_FLOAT_EQ(scroller.target().y, 0.0F);
  settle(scroller, milliseconds{32});
  scroller.grab(milliseconds{3000});
  scroller.drag(Size{.width = 0, .height = -20}, milliseconds{3016});
  scroller.release(Size{.width = 0, .height = -200}, milliseconds{3016});
  EXPECT_FLOAT_EQ(scroller.target().y, 250.0F);
}

TEST_F(ScrollerTest, AnimateSpringsJumpSnapsAndNudgeClamps) {
  Scroller scroller;
  scroller.configure(metrics());
  scroller.animate(Point{.x = 0, .y = 500}, motion::Animation::spring(0.4, 1.0),
                   milliseconds{0});
  EXPECT_TRUE(scroller.animating());
  EXPECT_EQ(scroller.phase(milliseconds{0}),
            composition::ScrollPhase::animating);
  scroller.advance(milliseconds{100});
  EXPECT_GT(scroller.offset().y, 0.0F);
  EXPECT_LT(scroller.offset().y, 500.0F);
  settle(scroller, milliseconds{116});
  EXPECT_FLOAT_EQ(scroller.offset().y, 500.0F);
  scroller.animate(Point{.x = 0, .y = 5000}, std::nullopt, milliseconds{3000});
  EXPECT_TRUE(scroller.idle());
  EXPECT_FLOAT_EQ(scroller.offset().y, 1600.0F);
  scroller.nudge(Point{.x = 0, .y = -2000}, milliseconds{3100});
  EXPECT_FLOAT_EQ(scroller.offset().y, 0.0F);
  EXPECT_EQ(scroller.phase(milliseconds{3110}),
            composition::ScrollPhase::interacting);
  EXPECT_EQ(scroller.phase(milliseconds{3300}), composition::ScrollPhase::idle);
}

TEST_F(ScrollerTest, IndicatorsShowWhileMovingAndFadeWhenIdle) {
  Scroller scroller;
  scroller.configure(metrics());
  EXPECT_FLOAT_EQ(scroller.indicators(milliseconds{0}), 0.0F);
  scroller.nudge(Point{.x = 0, .y = 50}, milliseconds{100});
  EXPECT_FLOAT_EQ(scroller.indicators(milliseconds{200}), 1.0F);
  EXPECT_FLOAT_EQ(scroller.indicators(milliseconds{700}), 1.0F);
  EXPECT_NEAR(scroller.indicators(milliseconds{850}), 0.5F, 0.01F);
  EXPECT_FLOAT_EQ(scroller.indicators(milliseconds{1100}), 0.0F);
  EXPECT_FALSE(scroller.moving(milliseconds{1100}));
}

TEST_F(ScrollerTest, ShrinkingContentClampsAnIdleOffset) {
  Scroller scroller;
  scroller.configure(metrics());
  scroller.animate(Point{.x = 0, .y = 1500}, std::nullopt, milliseconds{0});
  Scroller::Metrics smaller = metrics();
  smaller.content.height = 600;
  scroller.configure(smaller);
  EXPECT_FLOAT_EQ(scroller.offset().y, 200.0F);
}

}
