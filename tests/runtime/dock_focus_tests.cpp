#include "focus_fixture.hpp"

#include <arisa/components/control_size.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/axis.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/namespace.hpp>
#include <arisa/composition/overlay_input.hpp>
#include <arisa/composition/overlay_layer.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/runtime/overlay_behaviour.hpp>
#include <arisa/runtime/overlay_handle.hpp>
#include <arisa/runtime/overlays.hpp>
#include <arisa/structure/dock_bar.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <vector>

namespace arisa::runtime {

class DockFocusTest : public FocusFixture {
 protected:
  using Button = provider::Button;
  using DockBar = structure::DockBar;

  static constexpr composition::Namespace space{"dock-focus-test"};

  class Switching final : public composition::Component {
   public:
    static constexpr composition::StateKey<std::size_t> chosen{"chosen", 0};

    auto operator==(const Switching&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      const auto selection = context.state(chosen);
      return components::VStack(
                 components::HStack(button("panel.first")),
                 DockBar(composition::Binding<std::size_t>{selection}, items()))
          .on_command(Button::r, "Next", [selection] {
            selection.set((selection.get() + 1) % 3);
          });
    }
  };

  class Following final : public composition::Component {
   public:
    static constexpr composition::StateKey<std::size_t> chosen{"following", 0};

    auto operator==(const Following&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      const auto selection = context.state(chosen);
      return components::VStack(
          DockBar(composition::Binding<std::size_t>{selection}, items(), {},
                  DockBar::Options{.follows_focus = true}),
          button("selected." + std::to_string(selection.get())));
    }
  };

  static auto items() -> std::vector<DockBar::Item> {
    return {{.title = "One", .system_image = "tune"},
            {.title = "Two", .system_image = "article"},
            {.title = "Three", .system_image = "explore"}};
  }

  static auto dock(std::size_t selected, DockBar::Options options = {})
      -> composition::Element {
    return DockBar(composition::Binding<std::size_t>::constant(selected),
                   items(), {}, options);
  }

  static auto tab(std::size_t index) -> std::string {
    return DockBar::key_of(DockBar::tab_key, index);
  }

  void settle(Application& application) {
    for (int step = 0; step < 40; ++step) {
      ASSERT_TRUE(providers_.clock().advance(frame_step).has_value());
      frames(application, 1);
    }
  }

  provider::headless::HeadlessProviderFactory providers_;
};

TEST_F(DockFocusTest, AModalOverlayWithADockFocusesTheSelectedTab) {
  Application application = start(providers_, button("under"));
  OverlayHandle shown = application.root().overlays().show(
      composition::OverlayLayer::dialog,
      components::VStack(button("panel.first"), dock(2)),
      OverlayBehaviour{.input = composition::OverlayInput::modal,
                       .alignment = composition::Alignment::bottom});
  settle(application);
  EXPECT_EQ(focused_key(application), tab(2));
}

TEST_F(DockFocusTest, EnteringTheDockFromAboveLandsOnTheSelectedTab) {
  Application application = start(
      providers_,
      components::VStack(components::HStack(button("panel.first")), dock(2)));
  EXPECT_EQ(focused_key(application), tab(2));
  press(providers_, application, Button::dpad_up);
  EXPECT_EQ(focused_key(application), "panel.first");
  press(providers_, application, Button::dpad_down);
  EXPECT_EQ(focused_key(application), tab(2));
}

TEST_F(DockFocusTest,
       AContainerThatPrefersDefaultFocusTakesItForItsFirstControl) {
  Application application = start(
      providers_,
      components::VStack(button("first"),
                         components::VStack(button("second"), button("third"))
                             .prefers_default_focus(space.id()))
          .focus_scope(space.id()));
  EXPECT_EQ(focused_key(application), "second");
}

TEST_F(DockFocusTest, AContainerPreferenceOutsideItsScopeIsIgnored) {
  Application application = start(
      providers_, components::VStack(button("first"),
                                     components::VStack(button("second"))
                                         .prefers_default_focus(space.id())));
  EXPECT_EQ(focused_key(application), "first");
}

TEST_F(DockFocusTest, AVerticalDockStacksItsTabsAndMovesWithUpAndDown) {
  Application application = start(
      providers_, dock(0, DockBar::Options{.axis = composition::Axis::vertical})
                      .frame({.height = 400.0F}));
  const provider::PixelPoint first = centre(application, tab(0));
  const provider::PixelPoint second = centre(application, tab(1));
  EXPECT_NEAR(first.x, second.x, 0.5F);
  EXPECT_GT(second.y, first.y);
  EXPECT_EQ(focused_key(application), tab(0));
  press(providers_, application, Button::dpad_down);
  EXPECT_EQ(focused_key(application), tab(1));
}

TEST_F(DockFocusTest, ASmallControlSizeGivesTheCompactDock) {
  Application application =
      start(providers_, dock(0).control_size(components::ControlSize::small));
  const auto node = node_named(application, tab(1));
  ASSERT_TRUE(node.has_value());
  EXPECT_NEAR(node->get().frame().width, DockBar::compact_tab_width, 0.5F);
  EXPECT_NEAR(node->get().frame().height, DockBar::compact_tab_height, 0.5F);
}

TEST_F(DockFocusTest, FocusFollowsASelectionChangedFromOutsideTheDock) {
  Application application = start(providers_, Switching{});
  EXPECT_EQ(focused_key(application), tab(0));
  press(providers_, application, Button::r);
  EXPECT_EQ(focused_key(application), tab(1));
  press(providers_, application, Button::r);
  EXPECT_EQ(focused_key(application), tab(2));
  press(providers_, application, Button::dpad_up);
  EXPECT_EQ(focused_key(application), "panel.first");
  press(providers_, application, Button::r);
  EXPECT_EQ(focused_key(application), "panel.first");
}

TEST_F(DockFocusTest, AFollowingDockSelectsTheTabThatTakesFocus) {
  Application application = start(providers_, Following{});
  EXPECT_EQ(focused_key(application), tab(0));
  EXPECT_TRUE(node_named(application, "selected.0").has_value());
  press(providers_, application, Button::dpad_right);
  EXPECT_EQ(focused_key(application), tab(1));
  EXPECT_TRUE(node_named(application, "selected.1").has_value());
  press(providers_, application, Button::dpad_right);
  EXPECT_TRUE(node_named(application, "selected.2").has_value());
}

TEST_F(DockFocusTest, ADockWiderThanItsSpaceScrollsInsideIt) {
  std::vector<DockBar::Item> many;
  for (int index = 0; index < 12; ++index) {
    many.push_back(
        {.title = "Tab " + std::to_string(index), .system_image = "tune"});
  }
  Application application =
      start(providers_,
            components::VStack(
                DockBar(composition::Binding<std::size_t>::constant(0),
                        std::move(many), {}, DockBar::Options{.scrolls = true})
                    .id("dock"))
                .frame({.width = 300.0F}));
  settle(application);
  const auto bar = node_named(application, DockBar::bar_key);
  ASSERT_TRUE(bar.has_value());
  ASSERT_TRUE(bar->get().presented().has_value());
  EXPECT_LE(bar->get().presented()->bounds.width, 300.5F);
  for (int step = 0; step < 11; ++step) {
    press(providers_, application, Button::dpad_right);
  }
  EXPECT_EQ(focused_key(application), tab(11));
}

TEST_F(DockFocusTest, ADockThatFitsKeepsItsOwnWidth) {
  Application application = start(
      providers_,
      components::VStack(dock(0, DockBar::Options{.scrolls = true}).id("dock"))
          .frame({.width = 900.0F}));
  settle(application);
  const auto dock = node_named(application, "dock");
  const auto bar = node_named(application, DockBar::bar_key);
  ASSERT_TRUE(dock.has_value() && bar.has_value());
  ASSERT_TRUE(dock->get().presented().has_value() &&
              bar->get().presented().has_value());
  const DockBar::Metrics sizes = DockBar::metrics(false);
  const composition::Rect within = dock->get().presented()->bounds;
  const composition::Rect drawn = bar->get().presented()->bounds;
  EXPECT_NEAR(drawn.width,
              (3.0F * sizes.tab_width) + (2.0F * sizes.tab_spacing) +
                  (2.0F * sizes.padding),
              0.5F);
  EXPECT_NEAR(drawn.x - within.x,
              within.x + within.width - (drawn.x + drawn.width), 0.5F);
}

TEST_F(DockFocusTest, APaddedCompactDockStillScrollsInsideANarrowColumn) {
  std::vector<DockBar::Item> many;
  for (int index = 0; index < 12; ++index) {
    many.push_back(
        {.title = "Tab " + std::to_string(index), .system_image = "tune"});
  }
  Application application = start(
      providers_,
      components::VStack(
          DockBar(composition::Binding<std::size_t>::constant(0),
                  std::move(many), {}, DockBar::Options{.scrolls = true})
              .control_size(components::ControlSize::small)
              .labels_hidden()
              .padding(theme::Edge::top, theme::Space{8.0F})
              .id("dock"),
          components::VStack().frame({.max_width = composition::infinity,
                                      .max_height = composition::infinity}))
          .frame({.width = 300.0F}));
  settle(application);
  const auto bar = node_named(application, DockBar::bar_key);
  ASSERT_TRUE(bar.has_value());
  ASSERT_TRUE(bar->get().presented().has_value());
  EXPECT_LE(bar->get().presented()->bounds.width, 300.5F);
}
}
