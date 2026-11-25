#include <arisa/components/button.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/custom_hover_effect.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/gesture_input_kinds.hpp>
#include <arisa/composition/hover_effect_group.hpp>
#include <arisa/composition/hover_phase.hpp>
#include <arisa/composition/input_modality.hpp>
#include <arisa/composition/layer.hpp>
#include <arisa/composition/namespace.hpp>
#include <arisa/composition/overlay_layer.hpp>
#include <arisa/composition/pointer_style.hpp>
#include <arisa/composition/visibility.hpp>
#include <arisa/motion/hover_effect.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/provider/input/provider.hpp>

#include <runtime/pointer_cursor.hpp>

#include <scroll_fixture.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace arisa::runtime {

class PointerTest : public ScrollFixture {
 protected:
  static constexpr composition::Namespace linked{"pointer-test-linked"};

  struct Log {
    std::vector<bool> hovers;
    std::vector<composition::HoverPhase> phases;
  };

  static auto at(provider::PixelPoint position, bool left = false)
      -> provider::InputFrame {
    provider::InputFrame frame;
    frame.pointer = provider::PointerState{.position = position, .left = left};
    return frame;
  }

  static void point(provider::headless::HeadlessProviderFactory& providers,
                    Application& application, provider::PixelPoint position) {
    providers.input().push(
        at(provider::PixelPoint{.x = position.x + 1.0F, .y = position.y}));
    providers.input().push(at(position));
    frames(application, 3);
  }

  static auto tracked(const std::shared_ptr<Log>& log) -> composition::Element {
    return components::VStack(
        button("first"),
        components::ZStack()
            .frame({.width = 200.0F, .height = 100.0F})
            .on_hover([log](bool inside) { log->hovers.push_back(inside); })
            .on_continuous_hover([log](composition::HoverPhase phase) {
              log->phases.push_back(phase);
            })
            .id("area"));
  }

  static auto pointer_of(Application& application)
      -> interaction::InputRouter::Pointer {
    const auto pointer = application.interaction().pointer();
    EXPECT_TRUE(pointer.has_value());
    return pointer.value_or(interaction::InputRouter::Pointer{});
  }
};

TEST_F(PointerTest, OnHoverReportsEnteringAndLeaving) {
  provider::headless::HeadlessProviderFactory providers;
  const auto log = std::make_shared<Log>();
  Application application = start(providers, tracked(log));
  const composition::Rect area = presented_of(application, "area");
  point(providers, application,
        provider::PixelPoint{.x = area.x + 20.0F, .y = area.y + 30.0F});
  ASSERT_EQ(log->hovers.size(), 1U);
  EXPECT_TRUE(log->hovers.back());
  ASSERT_FALSE(log->phases.empty());
  const auto location = log->phases.back().location();
  ASSERT_TRUE(location.has_value());
  EXPECT_NEAR(location->x, 20.0F, 0.5F);
  EXPECT_NEAR(location->y, 30.0F, 0.5F);
  point(providers, application, centre(application, "first"));
  ASSERT_EQ(log->hovers.size(), 2U);
  EXPECT_FALSE(log->hovers.back());
  EXPECT_EQ(log->phases.back(), composition::HoverPhase::ended);
}

TEST_F(PointerTest, TouchAndTheControllerHideThePointerUntilItMoves) {
  provider::headless::HeadlessProviderFactory providers;
  const auto log = std::make_shared<Log>();
  Application application = start(providers, tracked(log));
  const composition::Rect area = presented_of(application, "area");
  const provider::PixelPoint inside{.x = area.x + 10.0F, .y = area.y + 10.0F};
  point(providers, application, inside);
  EXPECT_TRUE(application.interaction().pointing());
  EXPECT_TRUE(pointer_of(application).visible);
  press(providers, application, provider::Button::dpad_down);
  EXPECT_FALSE(application.interaction().pointing());
  EXPECT_FALSE(pointer_of(application).visible);
  EXPECT_EQ(application.interaction().modality(),
            composition::InputModality::controller);
  ASSERT_FALSE(log->hovers.empty());
  EXPECT_FALSE(log->hovers.back());
  point(providers, application, inside);
  EXPECT_TRUE(pointer_of(application).visible);
  EXPECT_TRUE(log->hovers.back());
  tap(providers, application, inside);
  EXPECT_FALSE(pointer_of(application).visible);
  EXPECT_EQ(application.interaction().modality(),
            composition::InputModality::touch);
}

TEST_F(PointerTest, TheInnermostPointerStyleWinsAndHiddenHidesIt) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers,
      components::VStack(
          components::VStack(
              button("plain"),
              button("link").pointer_style(composition::PointerStyle::link),
              button("hidden").pointer_visibility(
                  composition::Visibility::hidden))
              .pointer_style(composition::PointerStyle::column_resize()),
          button("outside")));
  point(providers, application, centre(application, "link"));
  EXPECT_EQ(pointer_of(application).style, composition::PointerStyle::link);
  point(providers, application, centre(application, "plain"));
  EXPECT_EQ(pointer_of(application).style,
            composition::PointerStyle::column_resize());
  point(providers, application, centre(application, "outside"));
  EXPECT_EQ(pointer_of(application).style, composition::PointerStyle::default_);
  ASSERT_TRUE(providers.input().presented_pointer().has_value());
  EXPECT_EQ(providers.input().presented_pointer()->shape,
            provider::PointerShape::arrow);
  point(providers, application, centre(application, "link"));
  EXPECT_EQ(providers.input().presented_pointer()->shape,
            provider::PointerShape::pointing_hand);
  point(providers, application, centre(application, "hidden"));
  EXPECT_FALSE(pointer_of(application).visible);
  EXPECT_FALSE(providers.input().presented_pointer()->visible);
}

TEST_F(PointerTest, StylesMapToTheSystemShapes) {
  using Shape = provider::PointerShape;
  using composition::PointerStyle;
  EXPECT_EQ(PointerCursor::shape_of(PointerStyle::horizontal_text),
            Shape::text);
  EXPECT_EQ(PointerCursor::shape_of(PointerStyle::grab_active),
            Shape::closed_hand);
  EXPECT_EQ(PointerCursor::shape_of(PointerStyle::column_resize(
                composition::HorizontalDirections::trailing)),
            Shape::resize_trailing);
  EXPECT_EQ(PointerCursor::shape_of(
                PointerStyle::row_resize(composition::VerticalDirections::up)),
            Shape::resize_top);
  EXPECT_EQ(PointerCursor::shape_of(PointerStyle::frame_resize(
                composition::FrameResizePosition::top_trailing)),
            Shape::resize_ascending);
  EXPECT_EQ(PointerCursor::shape_of(PointerStyle::frame_resize(
                composition::FrameResizePosition::bottom_leading,
                composition::FrameResizeDirections::outward)),
            Shape::resize_bottom_leading);
  EXPECT_EQ(PointerCursor::shape_of(PointerStyle::frame_resize(
                composition::FrameResizePosition::leading,
                composition::FrameResizeDirections::inward)),
            Shape::resize_trailing);
  EXPECT_EQ(PointerCursor::shape_of(PointerStyle::image(
                [] { return composition::Element{components::Text("x")}; },
                motion::UnitPoint::top_leading)),
            Shape::custom);
  EXPECT_TRUE(composition::GestureInputKinds::all.contains(
      composition::GestureInputKinds::pointer));
}

TEST_F(PointerTest, FocusFollowsTheHoveredControlWhilePointing) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers,
            components::VStack(button("a"), button("b"),
                               button("quiet").hover_effect_disabled(),
                               components::ZStack()
                                   .frame({.width = 200.0F, .height = 80.0F})
                                   .id("blank")));
  point(providers, application, centre(application, "b"));
  EXPECT_EQ(focused_key(application), "b");
  EXPECT_TRUE(application.interaction().focus_visible());
  point(providers, application, centre(application, "blank"));
  EXPECT_EQ(focused_key(application), "b");
  EXPECT_FALSE(application.interaction().focus_visible());
  point(providers, application, centre(application, "quiet"));
  EXPECT_EQ(focused_key(application), "b");
  EXPECT_FALSE(application.interaction().focus_visible());
  press(providers, application, provider::Button::dpad_up);
  EXPECT_EQ(focused_key(application), "a");
  EXPECT_TRUE(application.interaction().focus_visible());
}

TEST_F(PointerTest, HoveringALeaderActivatesItsGroup) {
  provider::headless::HeadlessProviderFactory providers;
  const composition::HoverEffectGroup leader{linked.id()};
  Application application = start(
      providers,
      components::VStack(
          button("lead").hover_effect(motion::HoverEffect::lift, leader),
          button("follow").hover_effect(
              motion::HoverEffect::lift,
              leader.behavior(
                  composition::HoverEffectGroup::Behavior::follows_group)),
          components::VStack(button("inner")).hover_effect_group().id("box")));
  point(providers, application, centre(application, "follow"));
  ASSERT_NE(application.interaction().hover_path(), nullptr);
  EXPECT_FALSE(application.interaction().hover_path()->active(leader));
  point(providers, application, centre(application, "lead"));
  EXPECT_TRUE(application.interaction().hover_path()->active(leader));
  point(providers, application, centre(application, "inner"));
  const auto box = node_of(application, "box");
  ASSERT_TRUE(box.has_value());
  EXPECT_TRUE(application.interaction().hover_path()->active(*box));
  EXPECT_TRUE(application.interaction().hover_path()->contains(
      *node_of(application, "inner")));
}

TEST_F(PointerTest, TheWheelScrollsTheViewUnderThePointer) {
  provider::headless::HeadlessProviderFactory providers;
  std::vector<composition::Element> rows;
  for (int index = 0; index < 30; ++index) {
    rows.push_back(button("row" + std::to_string(index)));
  }
  Application application = start(
      providers,
      components::VStack(
          components::ScrollView(
              components::VStack(components::VStackOptions{}, std::move(rows)))
              .frame({.width = 300.0F, .height = 200.0F})
              .id("list")));
  const provider::PixelPoint over = centre(application, "list");
  point(providers, application, over);
  const float before = offset(application, "list").y;
  provider::InputFrame wheel = at(over);
  wheel.pointer->wheel_y = -3.0F;
  providers.input().push(wheel);
  for (int index = 0; index < 40; ++index) {
    step(providers, application, frame_step);
  }
  EXPECT_GT(offset(application, "list").y, before + 10.0F);
}

TEST_F(PointerTest, TheApplicationDrawsTheCursorOnConsoles) {
  provider::headless::HeadlessProviderFactory providers;
  providers.input().set_pointer_host(provider::PointerHost::application);
  Application application = start(providers, components::VStack(button("a")));
  const auto cursor_layer = [&application] {
    std::size_t found = 0;
    for (const composition::NodeId id : application.root().tree().layers()) {
      const auto node = application.root().tree().node(id);
      if (node.has_value()) {
        if (const auto layer =
                node->get().content_as<composition::Layer::Content>();
            layer.has_value() &&
            layer->get().layer() == composition::OverlayLayer::pointer) {
          ++found;
        }
      }
    }
    return found;
  };
  EXPECT_EQ(cursor_layer(), 0U);
  point(providers, application, provider::PixelPoint{.x = 300.0F, .y = 300.0F});
  EXPECT_EQ(cursor_layer(), 1U);
  EXPECT_FALSE(providers.input().presented_pointer().has_value());
  press(providers, application, provider::Button::dpad_down);
  EXPECT_EQ(cursor_layer(), 0U);
}

}
