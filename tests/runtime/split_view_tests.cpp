#include <arisa/components/button.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/label.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/navigation_split_view_visibility.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/toolbar_item.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/with_animation.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/structure/navigation_split_view.hpp>
#include <arisa/structure/scaffold.hpp>
#include <arisa/structure/tab.hpp>
#include <arisa/structure/tab_view.hpp>
#include <arisa/structure/tab_view_style.hpp>

#include <focus_fixture.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace arisa::runtime {

using composition::Element;
using Visibility = composition::NavigationSplitViewVisibility;

class SplitViewTest : public FocusFixture {
 protected:
  struct Options {
    structure::SidebarCollapse forms{};
    bool fixed{false};
    bool scaffold{false};
    Visibility initial{Visibility::all};

    friend auto operator==(const Options&, const Options&) -> bool = default;
  };

  class Split final : public composition::Component {
   public:
    static constexpr composition::StateKey<std::optional<Visibility>> shown{
        "split-visibility", std::nullopt};

    explicit Split(Options options) : options_(options) {}

    auto operator==(const Split&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const auto state = context.state(shown);
      const Options options = options_;
      handle() = state;
      Element split = structure::NavigationSplitView(
          std::optional<structure::NavigationSplitView::Binding>{
              structure::NavigationSplitView::Binding{
                  .get =
                      [state, options] {
                        return state.get().value_or(options.initial);
                      },
                  .set = [state](Visibility value) { state.set(value); }}},
          components::VStack(components::Button("Alpha", [] {}).id("alpha"),
                             components::Button("Beta", [] {}).id("beta"))
              .frame({.max_width = composition::infinity}),
          components::VStack(components::Button("Detail", [] {}).id("detail"))
              .frame({.max_width = composition::infinity,
                      .max_height = composition::infinity}));
      split = std::move(split).sidebar_collapse(options.forms);
      if (options.fixed) {
        split = std::move(split).toolbar(composition::ToolbarRemoval{
            .removing = composition::ToolbarDefaultItemKind::sidebar_toggle});
      }
      if (options.scaffold) {
        return structure::Scaffold(std::move(split));
      }
      return split;
    }

    static auto handle()
        -> std::optional<composition::StateHandle<std::optional<Visibility>>>& {
      static std::optional<composition::StateHandle<std::optional<Visibility>>>
          held;
      return held;
    }

   private:
    Options options_;
  };

  class Rail final : public composition::Component {
   public:
    static constexpr composition::StateKey<int> page{"rail-page", 0};
    static constexpr composition::StateKey<Visibility> columns{
        "rail-columns", Visibility::icons_only};

    auto operator==(const Rail&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const auto selection = context.state(page);
      const auto visibility = context.state(columns);
      selected() = selection;
      return Element{
          structure::TabView(
              selection,
              structure::Tab(
                  "Home", "home", 0,
                  [] { return Element{components::Text("Home page")}; }),
              structure::Tab(
                  "Search", "search", 1,
                  [] { return Element{components::Text("Search page")}; }),
              structure::Tab(
                  "Settings", "settings", 2,
                  [] { return Element{components::Text("Settings page")}; }))
              .column_visibility(visibility)}
          .tab_view_style(structure::SidebarAdaptableTabViewStyle{})
          .sidebar_collapse(structure::SidebarCollapse{.detail_only = true,
                                                       .icons_only = true});
    }

    static auto selected() -> std::optional<composition::StateHandle<int>>& {
      static std::optional<composition::StateHandle<int>> held;
      return held;
    }
  };

  static auto frame_of(Application& application, std::string_view key)
      -> composition::Rect {
    const auto node = node_named(application, key);
    if (!node.has_value()) {
      return composition::Rect{};
    }
    return node->get().presented().has_value() ? node->get().presented()->bounds
                                               : node->get().frame();
  }

  void settle(Application& application) {
    for (int index = 0; index < 40; ++index) {
      ASSERT_TRUE(providers_.clock()
                      .advance(std::chrono::milliseconds{16})
                      .has_value());
      frames(application, 1);
    }
  }

  static auto disabled(Application& application, std::string_view key) -> bool {
    const composition::Tree& tree = application.root().tree();
    std::optional<composition::NodeId> at = node_of(application, key);
    while (at.has_value()) {
      const auto node = tree.node(*at);
      if (!node.has_value()) {
        return false;
      }
      if (node->get().interactions().disabled) {
        return true;
      }
      at = node->get().parent();
    }
    return false;
  }

  static auto keyed(Application& application, composition::ElementKey key)
      -> std::optional<composition::NodeId> {
    return application.root().tree().find(key);
  }

  static auto current() -> Visibility {
    return Split::handle()->get().value_or(Visibility::all);
  }

  static auto sidebar_width(Application& application) -> float {
    return frame_of(application, structure::NavigationSplitView::sidebar_id)
        .width;
  }

  provider::headless::HeadlessProviderFactory providers_;
};

TEST_F(SplitViewTest, EachVisibilitySizesTheSidebarColumn) {
  Application application =
      start(providers_,
            Split{Options{.forms = {.detail_only = true, .icons_only = true}}});
  settle(application);
  EXPECT_NEAR(sidebar_width(application), 144.0F, 0.5F);
  EXPECT_NEAR(
      frame_of(application, structure::NavigationSplitView::detail_id).x,
      144.0F, 0.5F);
  Split::handle()->set(Visibility::icons_only);
  settle(application);
  EXPECT_NEAR(sidebar_width(application), 64.0F, 0.5F);
  Split::handle()->set(Visibility::detail_only);
  settle(application);
  EXPECT_NEAR(sidebar_width(application), 0.0F, 0.5F);
  EXPECT_TRUE(disabled(application, "alpha"));
  EXPECT_FALSE(disabled(application, "detail"));
}

TEST_F(SplitViewTest, TheShortcutCyclesThroughTheAllowedForms) {
  Application application =
      start(providers_,
            Split{Options{.forms = {.detail_only = true, .icons_only = true}}});
  EXPECT_EQ(current(), Visibility::all);
  press(providers_, application, structure::NavigationSplitView::toggle_button);
  EXPECT_EQ(current(), Visibility::icons_only);
  press(providers_, application, structure::NavigationSplitView::toggle_button);
  EXPECT_EQ(current(), Visibility::detail_only);
  press(providers_, application, structure::NavigationSplitView::toggle_button);
  EXPECT_EQ(current(), Visibility::all);
  EXPECT_EQ(structure::NavigationSplitView::next(
                Visibility::all, {.detail_only = true, .icons_only = false}),
            Visibility::detail_only);
  EXPECT_EQ(
      structure::NavigationSplitView::next(
          Visibility::detail_only, {.detail_only = true, .icons_only = false}),
      Visibility::all);
  EXPECT_EQ(structure::NavigationSplitView::resolved(
                Visibility::icons_only,
                {.detail_only = true, .icons_only = false}, false),
            Visibility::detail_only);
}

TEST_F(SplitViewTest, TheContentPaneSpringsToItsNewWidth) {
  Application application = start(providers_, Split{Options{}});
  settle(application);
  const std::string_view pane = structure::NavigationSplitView::detail_id;
  const float before = frame_of(application, pane).x;
  press(providers_, application, structure::NavigationSplitView::toggle_button);
  ASSERT_TRUE(
      providers_.clock().advance(std::chrono::milliseconds{64}).has_value());
  frames(application, 1);
  const float moving = frame_of(application, pane).x;
  settle(application);
  const float after = frame_of(application, pane).x;
  EXPECT_LT(after, before - 100.0F);
  EXPECT_LT(moving, before - 1.0F);
  EXPECT_GT(moving, after + 1.0F);
}

TEST_F(SplitViewTest, RemovingTheToggleMakesTheSidebarFixed) {
  Application application =
      start(providers_,
            Split{Options{.forms = {.detail_only = true, .icons_only = true},
                          .fixed = true,
                          .scaffold = true,
                          .initial = Visibility::detail_only}});
  settle(application);
  EXPECT_NEAR(sidebar_width(application), 144.0F, 0.5F);
  press(providers_, application, structure::NavigationSplitView::toggle_button);
  settle(application);
  EXPECT_FALSE(Split::handle()->get().has_value());
  EXPECT_NEAR(sidebar_width(application), 144.0F, 0.5F);
  EXPECT_FALSE(node_of(application, structure::Scaffold::sidebar_toggle_key)
                   .has_value());
  EXPECT_FALSE(shows(application, "Sidebar"));
}

TEST_F(SplitViewTest, TheScaffoldShowsAToggleAndTheHintBarTheShortcut) {
  Application application = start(providers_, Split{Options{.scaffold = true}});
  settle(application);
  const auto toggle =
      node_of(application, structure::Scaffold::sidebar_toggle_key);
  ASSERT_TRUE(toggle.has_value());
  EXPECT_TRUE(shows(application, "Sidebar"));
  EXPECT_TRUE(application.root().activate(*toggle));
  frames(application, 2);
  EXPECT_EQ(current(), Visibility::detail_only);
}

TEST_F(SplitViewTest, TheRailShowsIconsAndSelectionFollowsFocus) {
  Application application = start(providers_, Rail{});
  settle(application);
  EXPECT_NEAR(sidebar_width(application), 64.0F, 0.5F);
  EXPECT_FALSE(shows(application, "Search"));
  const auto first = keyed(application, structure::TabView::item_key(0));
  ASSERT_TRUE(first.has_value());
  EXPECT_EQ(application.interaction().focused(), first);
  EXPECT_TRUE(shows(application, "Home"));
  press(providers_, application, provider::Button::dpad_down);
  settle(application);
  EXPECT_EQ(application.interaction().focused(),
            keyed(application, structure::TabView::item_key(1)));
  EXPECT_EQ(Rail::selected()->get(), 1);
  EXPECT_TRUE(shows(application, "Search"));
  EXPECT_FALSE(shows(application, "Home"));
  EXPECT_TRUE(shows(application, "Search page"));
  EXPECT_TRUE(
      node_of(application, structure::TabView::rail_label_id).has_value());
}

}
