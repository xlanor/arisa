#include <arisa/components/lazy_stack.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/controls/searchable.hpp>
#include <arisa/markup/markup.hpp>

#include <gtest/gtest.h>

#include <format>
#include <ranges>
#include <string>

namespace arisa::markup {

using composition::Element;

TEST(ScrollMarkupTest, ScrollViewTagDeclaresAScrollContainer) {
  const Element element =
      ScrollView(axes(Axes::horizontal), shows_indicators(false),
                 content_margins(theme::space::sm),
                 scroll_target_behavior(ScrollTargetBehavior::paging))(
          Text("one"), Text("two"));
  const auto& scrolling = element.interactions().scrolling;
  ASSERT_TRUE(scrolling.container.has_value());
  EXPECT_EQ(scrolling.container->axes, Axes::horizontal);
  EXPECT_FALSE(scrolling.container->shows_indicators);
  EXPECT_EQ(scrolling.target_behavior, ScrollTargetBehavior::paging);
  ASSERT_TRUE(scrolling.content_margins.has_value());
  EXPECT_EQ(element.children().size(), 1U);
}

TEST(ScrollMarkupTest, ScrollModifierAttributesDeclareTheirSettings) {
  struct Halves {
    void update_target(ScrollTarget& target,
                       const ScrollTargetContext& context) const {
      target.rect.y = context.container_size.height * 0.5F;
    }
  };
  const Element element = ScrollView(
      scroll_indicators(ScrollIndicatorVisibility::hidden, Axes::horizontal),
      scroll_indicators_flash(3),
      scroll_bounce_behavior(ScrollBounceBehavior::based_on_size),
      scroll_deceleration_rate(ScrollDecelerationRate::fast()),
      default_scroll_anchor(UnitPoint::bottom, ScrollAnchorRole::alignment),
      scroll_target_behavior(Halves{}))(Text("one"));
  const auto& scrolling = element.interactions().scrolling;
  EXPECT_EQ(scrolling.indicators.horizontal, ScrollIndicatorVisibility::hidden);
  EXPECT_FALSE(scrolling.indicators.vertical.has_value());
  ASSERT_TRUE(scrolling.flash.has_value());
  EXPECT_EQ(scrolling.flash->trigger, composition::ElementKey{3});
  EXPECT_EQ(scrolling.bounce.vertical, ScrollBounceBehavior::based_on_size);
  EXPECT_EQ(scrolling.deceleration, ScrollDecelerationRate::fast());
  EXPECT_EQ(scrolling.anchors.alignment, UnitPoint::bottom);
  EXPECT_FALSE(scrolling.anchors.initial_offset.has_value());
  EXPECT_TRUE(scrolling.custom_target_behavior.has_value());
}

TEST(ScrollMarkupTest, SearchAttributesWrapTheContent) {
  auto text = Binding<std::string>::constant("");
  const Element element =
      VStack(searchable(text, std::string{"Titles"}),
             search_suggestions([] { return Text("Star"); }))(Text("x"));
  EXPECT_EQ(element.type(),
            composition::ElementType::of<controls::Searchable::Content>());
}

TEST(ScrollMarkupTest, LazyStackTagsTakeForEachSources) {
  const Element vertical =
      LazyVStack(spacing(theme::space::xs), overscan(64.0F))(ForEach(
          std::views::iota(0, 1000), [](int index) { return index; },
          [](int index) { return Text(std::format("{}", index)); }));
  ASSERT_EQ(vertical.type(),
            composition::ElementType::of<components::LazyStack::Content>());
  const auto& content =
      static_cast<const components::LazyStack::Content&>(vertical.content());
  EXPECT_EQ(content.count(), 1000U);
  EXPECT_FLOAT_EQ(content.arrangement().overscan, 64.0F);
  EXPECT_EQ(content.arrangement().axis, composition::Axis::vertical);
  const Element horizontal = LazyHStack(alignment::top)(ForEach(
      std::views::iota(0, 3), [](int index) { return index; },
      [](int index) { return Text(std::format("{}", index)); }));
  const auto& sideways =
      static_cast<const components::LazyStack::Content&>(horizontal.content());
  EXPECT_EQ(sideways.arrangement().axis, composition::Axis::horizontal);
  EXPECT_EQ(sideways.arrangement().vertical,
            components::VerticalAlignment::top);
}

TEST(ScrollMarkupTest, GestureAttributesAttachRecognisers) {
  const Element element =
      VStack(on_tap_gesture(2, [] { return; }),
             simultaneous_gesture(DragGesture(0.0F)),
             high_priority_gesture(LongPressGesture(1.0)),
             content_shape(ContentShapeKinds::interaction |
                               ContentShapeKinds::focus_effect,
                           theme::Capsule()),
             scroll_transition([](VisualEffect content, ScrollTransitionPhase) {
               return content;
             }),
             visual_effect([](VisualEffect content, GeometryProxy) {
               return content;
             }))(Text("x"));
  const composition::Interactions& declared = element.interactions();
  ASSERT_EQ(declared.gestures.size(), 3U);
  EXPECT_EQ(declared.gestures[0].priority,
            composition::GesturePriority::normal);
  EXPECT_EQ(declared.gestures[1].priority,
            composition::GesturePriority::simultaneous);
  EXPECT_EQ(declared.gestures[2].priority, composition::GesturePriority::high);
  EXPECT_TRUE(declared.hit_shape.has_value());
  EXPECT_TRUE(declared.focus_shape.has_value());
  EXPECT_EQ(declared.effects.transitions.size(), 1U);
  EXPECT_EQ(declared.effects.effects.size(), 1U);
}

}
