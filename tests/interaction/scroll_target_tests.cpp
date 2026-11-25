#include <arisa/composition/any_scroll_target_behavior.hpp>
#include <arisa/composition/axes.hpp>
#include <arisa/composition/axis.hpp>
#include <arisa/composition/coordinate_space.hpp>
#include <arisa/composition/geometry_proxy.hpp>
#include <arisa/composition/paging_scroll_target_behavior.hpp>
#include <arisa/composition/point.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/composition/scroll_effects.hpp>
#include <arisa/composition/scroll_target.hpp>
#include <arisa/composition/scroll_target_behavior.hpp>
#include <arisa/composition/scroll_target_behavior_properties.hpp>
#include <arisa/composition/scroll_transition_configuration.hpp>
#include <arisa/composition/size.hpp>
#include <arisa/composition/view_aligned_scroll_target_behavior.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/scroll_content_offset_adjustment_behavior.hpp>
#include <arisa/motion/transaction.hpp>
#include <arisa/motion/unit_curve.hpp>
#include <arisa/motion/unit_point.hpp>

#include <interaction/scroller.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <optional>
#include <vector>

namespace arisa::interaction {

using composition::Point;
using composition::Rect;
using composition::ScrollTarget;
using composition::ScrollTargetContext;
using composition::Size;
using composition::ViewAlignedScrollTargetBehavior;
using std::chrono::milliseconds;
using Limit = ViewAlignedScrollTargetBehavior::LimitBehavior;
using Threshold = composition::ScrollTransitionConfiguration::Threshold;
using Configuration = composition::ScrollTransitionConfiguration;

class ScrollTargetTest : public ::testing::Test {
 protected:
  struct Limited {
    void update_target(ScrollTarget& target,
                       const ScrollTargetContext& context) const {
      static_cast<void>(target);
      static_cast<void>(context);
    }

    auto properties(
        const composition::ScrollTargetBehaviorPropertiesContext& context) const
        -> composition::ScrollTargetBehaviorProperties {
      return composition::ScrollTargetBehaviorProperties{
          .limits_scrolls = context.axes.contains(composition::Axis::vertical)};
    }

    friend auto operator==(const Limited&, const Limited&) -> bool = default;
  };

  struct Plain {
    void update_target(ScrollTarget& target,
                       const ScrollTargetContext& context) const {
      static_cast<void>(context);
      target.rect.y = 0.0F;
    }

    friend auto operator==(const Plain&, const Plain&) -> bool = default;
  };

  struct OneAtATime {
    void update_target(ScrollTarget& target,
                       const ScrollTargetContext& context) const {
      ViewAlignedScrollTargetBehavior{Limit::always_by_one}.update_target(
          target, context);
    }

    friend auto operator==(const OneAtATime&, const OneAtATime&)
        -> bool = default;
  };

  static auto metrics() -> Scroller::Metrics {
    Scroller::Metrics made{
        .viewport = Size{.width = 300, .height = 400},
        .content = Size{.width = 300, .height = 2000},
        .axes = composition::Axes::vertical,
        .behavior = composition::ScrollTargetBehavior::view_aligned,
        .targets = {}};
    for (int index = 0; index < 20; ++index) {
      made.targets.push_back(static_cast<float>(index) * 100.0F);
      made.extents.push_back(100.0F);
    }
    return made;
  }

  static auto flung(const Scroller::Metrics& chosen, float pulled, float speed)
      -> float {
    Scroller scroller;
    scroller.configure(chosen);
    scroller.grab(milliseconds{0});
    scroller.drag(Size{.width = 0, .height = -pulled}, milliseconds{16});
    scroller.release(Size{.width = 0, .height = -speed}, milliseconds{16});
    return scroller.target().y;
  }

  static auto phase(const Configuration& configuration, float top) -> double {
    const composition::ScrollEffects::ScrollTransition transition{
        .configuration = configuration};
    return transition.shaped(transition.progress(top, top + 40.0F, 100.0F));
  }

  static auto context(std::vector<Rect>& views, Point start, Point current,
                      Size velocity) -> ScrollTargetContext {
    return ScrollTargetContext{
        .original_target = ScrollTarget{},
        .velocity = velocity,
        .content_size = Size{.width = 300, .height = 2000},
        .container_size = Size{.width = 300, .height = 400},
        .axes = composition::Axes::vertical,
        .starting_offset = start,
        .current_offset = current,
        .aligned_views = views,
        .aligned_margin = 0.0F,
        .horizontally_compact = true};
  }
};

TEST_F(ScrollTargetTest, PropertiesDefaultToUnlimitedAndBuiltInsDescribeThem) {
  const composition::ScrollTargetBehaviorProperties plain{};
  EXPECT_FALSE(plain.limits_scrolls);
  EXPECT_EQ(plain, composition::ScrollTargetBehaviorProperties{});
  const composition::ScrollTargetBehaviorPropertiesContext compact{
      .axes = composition::Axes::horizontal, .horizontally_compact = true};
  const composition::ScrollTargetBehaviorPropertiesContext regular{
      .axes = composition::Axes::horizontal, .horizontally_compact = false};
  EXPECT_TRUE(composition::PagingScrollTargetBehavior{}
                  .properties(compact)
                  .limits_scrolls);
  EXPECT_TRUE(
      ViewAlignedScrollTargetBehavior{}.properties(compact).limits_scrolls);
  EXPECT_FALSE(
      ViewAlignedScrollTargetBehavior{}.properties(regular).limits_scrolls);
  EXPECT_TRUE(ViewAlignedScrollTargetBehavior{Limit::always}
                  .properties(regular)
                  .limits_scrolls);
  EXPECT_TRUE(ViewAlignedScrollTargetBehavior{Limit::always_by_few}
                  .properties(regular)
                  .limits_scrolls);
  EXPECT_TRUE(ViewAlignedScrollTargetBehavior{Limit::always_by_one}
                  .properties(regular)
                  .limits_scrolls);
  EXPECT_FALSE(ViewAlignedScrollTargetBehavior{Limit::never}
                   .properties(compact)
                   .limits_scrolls);
  EXPECT_EQ(
      ViewAlignedScrollTargetBehavior{Limit::automatic}.resolved_limit(true),
      Limit::always);
  EXPECT_EQ(ViewAlignedScrollTargetBehavior{}.resolved_limit(false),
            Limit::never);
}

TEST_F(ScrollTargetTest, AnyBehaviorForwardsPropertiesAndExposesItsBase) {
  const composition::AnyScrollTargetBehavior limited{Limited{}};
  const composition::AnyScrollTargetBehavior plain{Plain{}};
  const composition::ScrollTargetBehaviorPropertiesContext vertical{
      .axes = composition::Axes::vertical, .horizontally_compact = false};
  EXPECT_TRUE(limited.properties(vertical).limits_scrolls);
  EXPECT_FALSE(plain.properties(vertical).limits_scrolls);
  EXPECT_TRUE(limited.base<Limited>().has_value());
  EXPECT_FALSE(limited.base<Plain>().has_value());
  ASSERT_TRUE(plain.base<Plain>().has_value());
  ScrollTarget target{.rect = Rect{.x = 0, .y = 50, .width = 10, .height = 10},
                      .anchor = std::nullopt};
  plain.base<Plain>()->get().update_target(target, ScrollTargetContext{});
  EXPECT_FLOAT_EQ(target.rect.y, 0.0F);
  const composition::AnyScrollTargetBehavior aligned{
      ViewAlignedScrollTargetBehavior{motion::UnitPoint::center}};
  ASSERT_TRUE(aligned.base<ViewAlignedScrollTargetBehavior>().has_value());
  EXPECT_EQ(aligned.base<ViewAlignedScrollTargetBehavior>()->get().anchor(),
            motion::UnitPoint::center);
  EXPECT_TRUE(aligned.properties(vertical).limits_scrolls == false);
}

TEST_F(ScrollTargetTest, PagingUpdatesTargetsToTheNearestPageWithinOne) {
  std::vector<Rect> none;
  ScrollTarget target{
      .rect = Rect{.x = 0, .y = 1390, .width = 300, .height = 400},
      .anchor = motion::UnitPoint::center};
  composition::PagingScrollTargetBehavior{}.update_target(
      target,
      context(none, Point{.x = 0, .y = 400}, Point{.x = 0, .y = 420}, Size{}));
  EXPECT_FLOAT_EQ(target.rect.y, 800.0F);
  EXPECT_FALSE(target.anchor.has_value());
  target.rect.y = 590.0F;
  composition::PagingScrollTargetBehavior{}.update_target(
      target,
      context(none, Point{.x = 0, .y = 400}, Point{.x = 0, .y = 420}, Size{}));
  EXPECT_FLOAT_EQ(target.rect.y, 400.0F);
}

TEST_F(ScrollTargetTest, ViewAlignedAnchorsTheChosenViewAndLimitsItsReach) {
  std::vector<Rect> views;
  for (int index = 0; index < 20; ++index) {
    views.push_back(Rect{.x = 0,
                         .y = static_cast<float>(index) * 100.0F,
                         .width = 300,
                         .height = 100});
  }
  ScrollTarget target{
      .rect = Rect{.x = 0, .y = 60, .width = 300, .height = 400},
      .anchor = std::nullopt};
  ViewAlignedScrollTargetBehavior{motion::UnitPoint::center}.update_target(
      target, context(views, Point{}, Point{.x = 0, .y = 60}, Size{}));
  EXPECT_FLOAT_EQ(target.rect.y, 50.0F);
  target.rect.y = 60.0F;
  ViewAlignedScrollTargetBehavior{Limit::never, motion::UnitPoint::bottom}
      .update_target(target,
                     context(views, Point{}, Point{.x = 0, .y = 60}, Size{}));
  EXPECT_FLOAT_EQ(target.rect.y, 100.0F);
  target.rect.y = 1240.0F;
  ViewAlignedScrollTargetBehavior{Limit::always_by_one}.update_target(
      target, context(views, Point{}, Point{.x = 0, .y = 20},
                      Size{.width = 0, .height = 900}));
  EXPECT_FLOAT_EQ(target.rect.y, 100.0F);
  target.rect.y = 1240.0F;
  ViewAlignedScrollTargetBehavior{Limit::always_by_few}.update_target(
      target, context(views, Point{}, Point{.x = 0, .y = 20},
                      Size{.width = 0, .height = 900}));
  EXPECT_FLOAT_EQ(
      target.rect.y,
      static_cast<float>(ViewAlignedScrollTargetBehavior::few_views) * 100.0F);
  target.rect.y = 1240.0F;
  ViewAlignedScrollTargetBehavior{Limit::never}.update_target(
      target, context(views, Point{}, Point{.x = 0, .y = 20},
                      Size{.width = 0, .height = 900}));
  EXPECT_FLOAT_EQ(target.rect.y, 1200.0F);
}

TEST_F(ScrollTargetTest, TheScrollerHonoursLimitsScrolls) {
  Scroller::Metrics unlimited = metrics();
  unlimited.aligned = ViewAlignedScrollTargetBehavior{Limit::never};
  EXPECT_GT(flung(unlimited, 20.0F, 3000.0F), 800.0F);
  Scroller::Metrics limited = metrics();
  limited.aligned = ViewAlignedScrollTargetBehavior{Limit::always};
  EXPECT_FLOAT_EQ(flung(limited, 20.0F, 3000.0F), 400.0F);
  Scroller::Metrics automatic = metrics();
  automatic.horizontally_compact = false;
  EXPECT_GT(flung(automatic, 20.0F, 3000.0F), 800.0F);
  automatic.horizontally_compact = true;
  EXPECT_FLOAT_EQ(flung(automatic, 20.0F, 3000.0F), 400.0F);
  Scroller::Metrics custom = metrics();
  custom.behavior = composition::ScrollTargetBehavior::standard;
  custom.custom = composition::AnyScrollTargetBehavior{Limited{}};
  EXPECT_FLOAT_EQ(flung(custom, 20.0F, 3000.0F), 400.0F);
  custom.custom = composition::AnyScrollTargetBehavior{OneAtATime{}};
  EXPECT_FLOAT_EQ(flung(custom, 20.0F, 3000.0F), 100.0F);
  Scroller::Metrics centred = metrics();
  centred.aligned =
      ViewAlignedScrollTargetBehavior{Limit::never, motion::UnitPoint::center};
  EXPECT_FLOAT_EQ(flung(centred, 60.0F, 0.0F), 50.0F);
}

TEST_F(ScrollTargetTest, CarryKeepsAFlingsVelocityAcrossAJump) {
  Scroller scroller;
  Scroller::Metrics plain = metrics();
  plain.behavior = composition::ScrollTargetBehavior::standard;
  scroller.configure(plain);
  scroller.grab(milliseconds{0});
  scroller.drag(Size{.width = 0, .height = -20}, milliseconds{16});
  scroller.release(Size{.width = 0, .height = -800}, milliseconds{16});
  scroller.advance(milliseconds{48});
  ASSERT_TRUE(scroller.decelerating());
  const float speed = scroller.velocity().height;
  const float before = scroller.offset().y;
  EXPECT_TRUE(scroller.carry(Point{.x = 0, .y = 1000}, milliseconds{48}));
  EXPECT_TRUE(scroller.decelerating());
  EXPECT_FLOAT_EQ(scroller.offset().y, 1000.0F);
  EXPECT_FLOAT_EQ(scroller.velocity().height, speed);
  scroller.advance(milliseconds{64});
  EXPECT_GT(scroller.offset().y, 1000.0F);
  EXPECT_GT(before, 0.0F);
  Scroller resting;
  resting.configure(plain);
  EXPECT_FALSE(resting.carry(Point{.x = 0, .y = 300}, milliseconds{0}));
}

TEST_F(ScrollTargetTest, TransactionsCarryScrollPreferences) {
  motion::Transaction transaction;
  EXPECT_FALSE(transaction.scroll_target_anchor().has_value());
  EXPECT_FALSE(transaction.scroll_position_update_preserves_velocity());
  EXPECT_EQ(transaction.scroll_content_offset_adjustment_behavior(),
            motion::ScrollContentOffsetAdjustmentBehavior::automatic);
  transaction.set_scroll_target_anchor(motion::UnitPoint::center);
  transaction.set_scroll_position_update_preserves_velocity(true);
  transaction.set_scroll_content_offset_adjustment_behavior(
      motion::ScrollContentOffsetAdjustmentBehavior::disabled);
  EXPECT_EQ(transaction.scroll_target_anchor(), motion::UnitPoint::center);
  EXPECT_TRUE(transaction.scroll_position_update_preserves_velocity());
  EXPECT_EQ(transaction.scroll_content_offset_adjustment_behavior(),
            motion::ScrollContentOffsetAdjustmentBehavior::disabled);
}

TEST_F(ScrollTargetTest, TransitionThresholdsMatchTheSwiftUIReference) {
  constexpr double tolerance = 0.011;
  const Configuration linear =
      Configuration::interactive(motion::UnitCurve::linear);
  const Configuration eased = Configuration::interactive();
  EXPECT_EQ(eased.timing_curve(), motion::UnitCurve::ease_in_out);
  EXPECT_EQ(eased.threshold(), Threshold::visible());
  EXPECT_NEAR(phase(eased, -30.0F), -0.87, tolerance);
  EXPECT_NEAR(phase(eased, -20.0F), -0.5, tolerance);
  EXPECT_NEAR(phase(eased, 70.0F), 0.13, tolerance);
  EXPECT_NEAR(phase(eased, 90.0F), 0.87, tolerance);
  EXPECT_NEAR(phase(eased, 30.0F), 0.0, tolerance);
  EXPECT_NEAR(phase(linear, -30.0F), -0.75, tolerance);
  EXPECT_NEAR(phase(linear, 65.0F), 0.125, tolerance);
  EXPECT_NEAR(phase(linear, 95.0F), 0.875, tolerance);
  const Configuration half = linear.threshold(Threshold::visible(0.5));
  EXPECT_NEAR(phase(half, -30.0F), -0.5, tolerance);
  EXPECT_NEAR(phase(half, -20.0F), 0.0, tolerance);
  EXPECT_NEAR(phase(half, 90.0F), 0.5, tolerance);
  const Configuration hidden = linear.threshold(Threshold::hidden());
  EXPECT_NEAR(phase(hidden, -30.0F), 1.0, tolerance);
  EXPECT_NEAR(phase(hidden, 30.0F), 1.0, tolerance);
  EXPECT_NEAR(phase(hidden, 90.0F), 1.0, tolerance);
  const Configuration centered = linear.threshold(Threshold::centered());
  EXPECT_NEAR(phase(centered, -30.0F), -0.855, tolerance);
  EXPECT_NEAR(phase(centered, 25.0F), -0.07, tolerance);
  EXPECT_NEAR(phase(centered, 30.0F), 0.0, tolerance);
  EXPECT_NEAR(phase(centered, 35.0F), 0.07, tolerance);
  EXPECT_NEAR(phase(centered, 65.0F), 0.5, tolerance);
  EXPECT_NEAR(phase(centered, 95.0F), 0.93, tolerance);
  EXPECT_NEAR(phase(linear.threshold(Threshold::centered().inset(10.0)), 65.0F),
              0.5, tolerance);
  const Configuration inset =
      linear.threshold(Threshold::visible().inset(10.0));
  EXPECT_NEAR(phase(inset, -30.0F), -0.8, tolerance);
  EXPECT_NEAR(phase(inset, 0.0F), -0.2, tolerance);
  EXPECT_NEAR(phase(inset, 10.0F), 0.0, tolerance);
  EXPECT_NEAR(phase(inset, 60.0F), 0.2, tolerance);
  EXPECT_NEAR(phase(inset, 90.0F), 0.8, tolerance);
  const Configuration outset =
      linear.threshold(Threshold::visible().inset(-10.0));
  EXPECT_NEAR(phase(outset, -30.0F), -0.665, tolerance);
  EXPECT_NEAR(phase(outset, -10.0F), 0.0, tolerance);
  EXPECT_NEAR(phase(outset, 80.0F), 0.335, tolerance);
  EXPECT_NEAR(phase(outset, 90.0F), 0.665, tolerance);
  const Configuration hidden_inset =
      linear.threshold(Threshold::hidden().inset(10.0));
  EXPECT_NEAR(phase(hidden_inset, 90.0F), 0.0, tolerance);
  EXPECT_NEAR(phase(hidden_inset, 95.0F), 0.5, tolerance);
  const Configuration between = linear.threshold(
      Threshold::visible().interpolated(Threshold::hidden(), 0.5));
  EXPECT_EQ(between.threshold(), Threshold::visible(0.5));
  EXPECT_NEAR(phase(between, 90.0F), 0.5, tolerance);
  const Configuration toward = linear.threshold(
      Threshold::hidden().interpolated(Threshold::centered(), 0.5));
  EXPECT_NEAR(phase(toward, -30.0F), -0.715, tolerance);
  EXPECT_NEAR(phase(toward, -10.0F), -0.145, tolerance);
  EXPECT_NEAR(phase(toward, 0.0F), 0.0, tolerance);
  EXPECT_NEAR(phase(toward, 70.0F), 0.145, tolerance);
  EXPECT_NEAR(phase(toward, 95.0F), 0.855, tolerance);
}

TEST_F(ScrollTargetTest, AnimatedTransitionsStepPastTheirThreshold) {
  const Configuration animated =
      Configuration::animated(motion::Animation::linear(0.01));
  EXPECT_EQ(animated.threshold(), Threshold::visible(0.5));
  EXPECT_EQ(phase(animated, -30.0F), -1.0);
  EXPECT_EQ(phase(animated, -20.0F), 0.0);
  EXPECT_EQ(phase(animated, 80.0F), 0.0);
  EXPECT_EQ(phase(animated, 90.0F), 1.0);
  const Configuration full = animated.threshold(Threshold::visible());
  EXPECT_EQ(phase(full, -10.0F), -1.0);
  EXPECT_EQ(phase(full, 0.0F), 0.0);
  EXPECT_EQ(phase(full, 60.0F), 0.0);
  EXPECT_EQ(phase(full, 65.0F), 1.0);
  const Configuration centered = animated.threshold(Threshold::centered());
  EXPECT_EQ(phase(centered, 25.0F), -1.0);
  EXPECT_EQ(phase(centered, 30.0F), 0.0);
  EXPECT_EQ(phase(centered, 35.0F), 1.0);
  EXPECT_EQ(phase(Configuration::identity(), 90.0F), 0.0);
  const Configuration slowed =
      Configuration::animated().animation(motion::Animation::linear(2.0));
  EXPECT_EQ(slowed.animation(), motion::Animation::linear(2.0));
  EXPECT_EQ(slowed.mode(), Configuration::Mode::animated);
}

TEST_F(ScrollTargetTest, EachEdgeUsesItsOwnTransitionConfiguration) {
  const composition::ScrollEffects::ScrollTransition transition{
      .configuration = Configuration::identity(),
      .bottom_trailing = Configuration::interactive(motion::UnitCurve::linear)};
  EXPECT_EQ(transition.shaped(transition.progress(-30.0F, 10.0F, 100.0F)), 0.0);
  EXPECT_NEAR(transition.shaped(transition.progress(80.0F, 120.0F, 100.0F)),
              0.5, 1.0e-9);
  const composition::ScrollEffects::ScrollTransition flipped{
      .configuration = Configuration::interactive(motion::UnitCurve::linear),
      .bottom_trailing = Configuration::identity()};
  EXPECT_NEAR(flipped.shaped(flipped.progress(-20.0F, 20.0F, 100.0F)), -0.5,
              1.0e-9);
  EXPECT_EQ(flipped.shaped(flipped.progress(80.0F, 120.0F, 100.0F)), 0.0);
}

TEST_F(ScrollTargetTest, AnAxisScrollViewSpaceMeasuresAgainstThatAxis) {
  const Rect outer{.x = 0, .y = 100, .width = 400, .height = 300};
  const Rect inner{.x = 50, .y = 150, .width = 300, .height = 80};
  const composition::GeometryProxy proxy{
      Rect{.x = 70, .y = 160, .width = 40, .height = 40},
      inner,
      composition::GeometryProxy::ScrollViews{.horizontal = inner,
                                              .vertical = outer},
      nullptr,
      nullptr,
      composition::LayoutDirection::left_to_right};
  const composition::CoordinateSpace vertical =
      composition::CoordinateSpace::scroll_view_along(
          composition::Axis::vertical);
  EXPECT_EQ(vertical.kind(), composition::CoordinateSpace::Kind::scroll_view);
  EXPECT_EQ(vertical.axis(), composition::Axis::vertical);
  EXPECT_FALSE(composition::CoordinateSpace::scroll_view.axis().has_value());
  EXPECT_FLOAT_EQ(proxy.frame(vertical).y, 60.0F);
  EXPECT_FLOAT_EQ(proxy
                      .frame(composition::CoordinateSpace::scroll_view_along(
                          composition::Axis::horizontal))
                      .x,
                  20.0F);
  EXPECT_FLOAT_EQ(proxy.frame(composition::CoordinateSpace::scroll_view).y,
                  10.0F);
  EXPECT_EQ(proxy.bounds(vertical), outer);
  const composition::GeometryProxy flat{
      Rect{.x = 70, .y = 160, .width = 40, .height = 40}, inner};
  EXPECT_FLOAT_EQ(flat.frame(vertical).y, 160.0F);
}

}
