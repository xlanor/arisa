#include <arisa/components/accessibility_tree.hpp>
#include <arisa/components/button.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/user_interface_size_class.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/search_environment.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/controls/picker.hpp>
#include <arisa/controls/picker_style.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/structure/tab.hpp>
#include <arisa/structure/tab_bar_placement.hpp>
#include <arisa/structure/tab_view.hpp>
#include <arisa/structure/tab_view_customization.hpp>
#include <arisa/structure/tab_view_style.hpp>

#include <scroll_fixture.hpp>

#include <gtest/gtest.h>

#include <format>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace arisa::runtime {

using composition::Element;
using structure::Tab;
using structure::TabView;

class TabParityTest : public ScrollFixture {
 protected:
  struct Seen {
    std::optional<structure::TabBarPlacement> placement;
    std::optional<structure::TabViewBottomAccessoryPlacement> accessory;
    bool sidebar_available{false};
    bool sections{false};
    composition::SearchEnvironment::Activation activation;
  };

  static auto seen() -> Seen& {
    static Seen value;
    return value;
  }

  class Reader final : public composition::Component {
   public:
    explicit Reader(std::string text) : text_(std::move(text)) {}

    auto operator==(const Reader&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      Seen& into = seen();
      into.placement = context.environment(TabView::bar_placement);
      into.sidebar_available = context.environment(TabView::sidebar_available);
      into.sections = context.environment(TabView::showing_sections);
      into.activation =
          context.environment(composition::SearchEnvironment::activation);
      return components::Text(text_);
    }

   private:
    std::string text_;
  };

  class AccessoryReader final : public composition::Component {
   public:
    auto operator==(const AccessoryReader&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      seen().accessory = context.environment(TabView::accessory_placement);
      return components::Text("Now playing");
    }
  };

  static auto page(std::string text) -> Tab::Builder {
    return [text] { return Element{Reader{text}}; };
  }

  static auto key(std::size_t index) -> std::string {
    return TabView::item_key(index).description();
  }

  static auto traits_of(const Tab& tab) -> structure::TabTraits {
    const auto content = Tab::content(tab);
    EXPECT_TRUE(content.has_value());
    return content.has_value() ? content->get().traits()
                               : structure::TabTraits{};
  }

  static auto decorations_of(const Tab& tab) -> std::size_t {
    const auto content = Tab::content(tab);
    return content.has_value() ? content->get().decorations().size() : 0U;
  }

  static auto long_page() -> Tab::Builder {
    return [] {
      std::vector<Element> rows;
      for (int index = 0; index < 60; ++index) {
        rows.push_back(
            components::Text(std::format("line {}", index))
                .frame({.height = 44.0F, .max_width = composition::infinity}));
      }
      return Element{components::ScrollView(components::VStack(std::move(rows)))
                         .frame({.max_width = composition::infinity,
                                 .max_height = composition::infinity})};
    };
  }
};

TEST_F(TabParityTest, EveryInitialiserBuildsAnItem) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers,
      TabView(Tab("Asset", structure::TabImage{"tile"}, page("asset page")),
              Tab("Valued", structure::TabImage{"tile"}, 1, page("valued")),
              Tab("Roled", structure::TabImage{"tile"}, 2,
                  structure::TabRole::search, page("roled")),
              Tab("Plain role", structure::TabImage{"tile"},
                  structure::TabRole::search, page("plain role")),
              Tab("System role", "star", structure::TabRole::search,
                  page("system role")),
              Tab(page("bare")),
              Tab(structure::TabRole::search, page("searchy")),
              Tab(7, page("value only")),
              Tab(8, structure::TabRole::prominent, page("value role")),
              Tab(9, page("labelled"),
                  [] { return Element{components::Text("Custom face")}; })));
  frames(application, 4);
  EXPECT_TRUE(shows(application, "asset page"));
  EXPECT_TRUE(shows(application, "Asset"));
  EXPECT_TRUE(shows(application, "Valued"));
  EXPECT_TRUE(shows(application, "Custom face"));
  EXPECT_TRUE(node_of(application, TabView::prominent_id).has_value());
  for (std::size_t index = 0; index < 10; ++index) {
    if (index == 8) {
      continue;
    }
    EXPECT_TRUE(node_of(application, key(index)).has_value()) << index;
  }
  EXPECT_EQ(traits_of(Tab(7, page("x"))).icon,
            structure::TabTraits::Icon::none);
  EXPECT_EQ(traits_of(Tab("A", structure::TabImage{"tile"}, page("x"))).icon,
            structure::TabTraits::Icon::asset);
}

TEST_F(TabParityTest, ProminentRoleSitsApartInItsOwnCapsule) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers, TabView(Tab("Home", "home", 0, page("home")),
                         Tab("Compose", "edit", 1,
                             structure::TabRole::prominent, page("compose")),
                         Tab("Inbox", "mail", 2, page("inbox"))));
  frames(application, 4);
  ASSERT_TRUE(node_of(application, TabView::prominent_id).has_value());
  EXPECT_GT(presented_of(application, TabView::prominent_id).x,
            presented_of(application, "arisa.tab.bar").x);
  press(providers, application, provider::Button::r);
  settle(providers, application);
  EXPECT_TRUE(shows(application, "compose"));
}

TEST_F(TabParityTest, TraitsHideDisableAndPinItems) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers,
            TabView(Tab("One", "home", 0, page("one")),
                    Tab("Two", "star", 1, page("two")).disabled(),
                    Tab("Three", "mail", 2, page("three")).hidden(),
                    Tab("Four", "pin", 3, page("four"))
                        .tab_placement(structure::TabPlacement::pinned),
                    Tab("Five", "list", 4, page("five"))
                        .tab_placement(structure::TabPlacement::sidebar_only)));
  frames(application, 4);
  EXPECT_FALSE(node_of(application, key(2)).has_value());
  EXPECT_FALSE(node_of(application, key(4)).has_value());
  EXPECT_LT(frame_of(application, key(3)).x, frame_of(application, key(0)).x);
  tap(providers, application, centre(application, key(1)));
  settle(providers, application);
  EXPECT_TRUE(shows(application, "one"));
  EXPECT_FALSE(shows(application, "two"));
}

TEST_F(TabParityTest, AccessibilityAndHelpReachTheItem) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers, TabView(Tab("One", "home", 0, page("one"))
                                   .accessibility_identifier("tab-one")
                                   .accessibility_label("First tab")
                                   .accessibility_hint("Opens the first page")
                                   .accessibility_value("3 new")
                                   .help("Home page"),
                               Tab("Two", "star", 1, page("two"))
                                   .accessibility_identifier("tab-two")
                                   .accessibility_label("Ignored", false)));
  frames(application, 4);
  const components::AccessibilityTree tree{application.root().tree()};
  const auto one = tree.find("tab-one");
  ASSERT_TRUE(one.has_value());
  EXPECT_EQ(one->label(), "First tab");
  EXPECT_EQ(one->hint(), std::optional<std::string>{"Opens the first page"});
  EXPECT_EQ(one->value(), std::optional<std::string>{"3 new"});
  const auto two = tree.find("tab-two");
  ASSERT_TRUE(two.has_value());
  EXPECT_NE(two->label(), "Ignored");
  EXPECT_EQ(
      traits_of(Tab("One", "home", 0, page("one")).help("Home page")).help,
      std::optional<std::string>{"Home page"});
}

TEST_F(TabParityTest, ContextMenuAndPopoverDecorateTheItem) {
  provider::headless::HeadlessProviderFactory providers;
  static constexpr composition::StateKey<bool> shown{"tab-popover", true};
  static constexpr composition::StateKey<std::optional<int>> item{
      "tab-popover-item", std::optional<int>{4}};
  class Decorated final : public composition::Component {
   public:
    auto operator==(const Decorated&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const auto open = context.state(shown);
      const auto chosen = context.state(item);
      return TabView(
          Tab("One", "home", 0, page("one"))
              .context_menu([] { return components::Button("Pin tab", [] {}); })
              .popover(open, [] { return components::Text("Tab tip"); }),
          Tab("Two", "star", 1, page("two"))
              .popover(chosen, [](const int& value) {
                return components::Text(std::format("item {}", value));
              }));
    }
  };
  Application application = start(providers, Decorated{});
  settle(providers, application);
  EXPECT_TRUE(shows(application, "Tab tip"));
  EXPECT_EQ(decorations_of(Tab("One", "home", 0, page("one")).context_menu([] {
              return components::Button("x", [] {});
            })),
            1U);
}

TEST_F(TabParityTest, SidebarCarriesSectionActionsBottomBarAndToolbar) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers,
      TabView(Tab("Home", "home", 0, page("home")),
              structure::TabSection("Library",
                                    Tab("Songs", "music", 1, page("songs")),
                                    Tab("Albums", "album", 2, page("albums")))
                  .section_actions(
                      [] { return components::Button("Add playlist", [] {}); }))
          .tab_view_style(structure::SidebarAdaptableTabViewStyle{})
          .tab_view_sidebar_bottom_bar(
              [] { return components::Button("Account", [] {}); })
          .content_toolbar(structure::ContentToolbarPlacement::tab_view_sidebar,
                           [] { return components::Button("Filter", [] {}); }));
  frames(application, 4);
  EXPECT_TRUE(shows(application, "Add playlist"));
  ASSERT_TRUE(node_of(application, TabView::sidebar_bottom_id).has_value());
  ASSERT_TRUE(node_of(application, TabView::sidebar_toolbar_id).has_value());
  EXPECT_GT(frame_of(application, TabView::sidebar_bottom_id).y,
            frame_of(application, key(2)).y);
  EXPECT_LT(frame_of(application, TabView::sidebar_toolbar_id).y,
            frame_of(application, key(0)).y);
  EXPECT_EQ(seen().placement, std::optional<structure::TabBarPlacement>{
                                  structure::TabBarPlacement::sidebar});
  EXPECT_TRUE(seen().sidebar_available);
  EXPECT_TRUE(seen().sections);
}

TEST_F(TabParityTest, DefaultPlacementsChooseTheTabBar) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers, TabView(Tab("Home", "home", 0, page("home")),
                         Tab("Two", "star", 1, page("two")))
                     .tab_view_style(structure::SidebarAdaptableTabViewStyle{})
                     .default_adaptable_tab_bar_placement(
                         structure::AdaptableTabBarPlacement::tab_bar));
  frames(application, 4);
  EXPECT_EQ(seen().placement, std::optional<structure::TabBarPlacement>{
                                  structure::TabBarPlacement::top_bar});
  EXPECT_TRUE(seen().sidebar_available);
  EXPECT_FALSE(seen().sections);
  EXPECT_TRUE(node_of(application, "arisa.tab.bar").has_value());
  provider::headless::HeadlessProviderFactory fixed;
  Application forced = start(
      fixed, TabView(Tab("Home", "home", 0, page("forced")),
                     Tab("Two", "star", 1, page("two")))
                 .tab_view_style(structure::SidebarAdaptableTabViewStyle{})
                 .default_tab_bar_placement(
                     structure::AdaptableTabBarPlacement::tab_bar));
  frames(forced, 3);
  EXPECT_TRUE(shows(forced, "forced"));
  EXPECT_FALSE(node_of(forced, "arisa.tab.sidebar").has_value());
  EXPECT_FALSE(seen().sidebar_available);
}

TEST_F(TabParityTest, BottomAccessoryExpandsThenGoesInlineWhenMinimized) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers,
      TabView(Tab("Feed", "home", 0, long_page()),
              Tab("Two", "star", 1, page("two")))
          .tab_view_bottom_accessory([] { return Element{AccessoryReader{}}; })
          .tab_bar_minimize_behavior(
              structure::TabBarMinimizeBehavior::on_scroll_down));
  frames(application, 4);
  ASSERT_TRUE(node_of(application, TabView::accessory_id).has_value());
  EXPECT_EQ(seen().accessory,
            std::optional<structure::TabViewBottomAccessoryPlacement>{
                structure::TabViewBottomAccessoryPlacement::expanded});
  EXPECT_TRUE(node_of(application, key(1)).has_value());
  drag(providers, application, {.x = 640.0F, .y = 600.0F},
       {.x = 640.0F, .y = 200.0F}, 8);
  settle(providers, application);
  EXPECT_FALSE(node_of(application, key(1)).has_value());
  EXPECT_EQ(seen().accessory,
            std::optional<structure::TabViewBottomAccessoryPlacement>{
                structure::TabViewBottomAccessoryPlacement::inline_placement});
  drag(providers, application, {.x = 640.0F, .y = 300.0F},
       {.x = 640.0F, .y = 420.0F}, 6);
  settle(providers, application);
  EXPECT_TRUE(node_of(application, key(1)).has_value());
  provider::headless::HeadlessProviderFactory disabled;
  Application off = start(
      disabled, TabView(Tab("Feed", "home", 0, page("feed")))
                    .tab_view_bottom_accessory(
                        false, [] { return components::Text("Hidden"); }));
  frames(off, 3);
  EXPECT_FALSE(node_of(off, TabView::accessory_id).has_value());
}

TEST_F(TabParityTest, NeverMinimizeKeepsTheBarWhileScrolling) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers,
      TabView(Tab("Feed", "home", 0, long_page()),
              Tab("Two", "star", 1, page("two")))
          .tab_bar_minimize_behavior(structure::TabBarMinimizeBehavior::never));
  frames(application, 4);
  drag(providers, application, {.x = 640.0F, .y = 600.0F},
       {.x = 640.0F, .y = 200.0F}, 8);
  settle(providers, application);
  EXPECT_TRUE(node_of(application, key(1)).has_value());
}

TEST_F(TabParityTest, SearchTabSelectionActivatesAndReturns) {
  provider::headless::HeadlessProviderFactory providers;
  static constexpr composition::StateKey<int> selected{"tab-search-selected",
                                                       0};
  class Searching final : public composition::Component {
   public:
    auto operator==(const Searching&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      return TabView(context.state(selected),
                     Tab("Home", "home", 0, page("home")),
                     Tab("Find", "search", 1, structure::TabRole::search,
                         page("find")))
          .tab_view_search_activation(
              structure::TabSearchActivation::search_tab_selection);
    }
  };
  Application application = start(providers, Searching{});
  frames(application, 3);
  EXPECT_FALSE(seen().activation.active());
  press(providers, application, provider::Button::r);
  settle(providers, application);
  EXPECT_TRUE(shows(application, "find"));
  EXPECT_TRUE(seen().activation.active());
  seen().activation.dismissed();
  settle(providers, application);
  EXPECT_TRUE(shows(application, "home"));
}

TEST_F(TabParityTest, TabCustomizationRecordsTabBarVisibility) {
  structure::TabViewCustomization customization;
  customization.set_tab_bar_visibility("a", composition::Visibility::hidden);
  customization.set_sidebar_visibility("b", composition::Visibility::visible);
  const structure::TabViewCustomization::TabCustomization a =
      customization.tab("a");
  EXPECT_EQ(a.tab_bar_visibility(), composition::Visibility::hidden);
  EXPECT_EQ(customization.tab("b").sidebar_visibility(),
            composition::Visibility::visible);
  const auto decoded =
      structure::TabViewCustomization::decoded(customization.encoded());
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(*decoded, customization);
  EXPECT_EQ(decoded->tab_bar_visibility("a"), composition::Visibility::hidden);
  const Tab traited =
      Tab("One", "home", 0, page("x"))
          .default_visibility(composition::Visibility::hidden,
                              structure::AdaptableTabBarPlacement::tab_bar)
          .customization_behavior(structure::TabCustomizationBehavior::disabled,
                                  structure::AdaptableTabBarPlacement::sidebar);
  EXPECT_EQ(traits_of(traited).tab_bar_default,
            composition::Visibility::hidden);
  EXPECT_EQ(traits_of(traited).sidebar_default,
            composition::Visibility::automatic);
  EXPECT_EQ(traits_of(traited).sidebar_behavior,
            std::optional<structure::TabCustomizationBehavior>{
                structure::TabCustomizationBehavior::disabled});
  EXPECT_FALSE(traits_of(traited).tab_bar_behavior.has_value());
}

TEST_F(TabParityTest, TabsPickerStyleDrawsLikeSegmented) {
  static constexpr composition::StateKey<int> choice{"tabs-picker", 0};
  class Picking final : public composition::Component {
   public:
    explicit Picking(controls::PickerStyle style) : style_(std::move(style)) {}

    auto operator==(const Picking&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      return controls::Picker("Shade", context.state(choice),
                              [] {
                                return components::Group(
                                    components::Text("Red").tag(0),
                                    components::Text("Green").tag(1));
                              })
          .picker_style(style_)
          .frame({.width = 300.0F})
          .id("picker");
    }

   private:
    controls::PickerStyle style_;
  };
  provider::headless::HeadlessProviderFactory providers;
  Application tabs = start(providers, Picking{controls::TabsPickerStyle{}});
  frames(tabs, 3);
  provider::headless::HeadlessProviderFactory other;
  Application segmented =
      start(other, Picking{controls::PickerStyle::segmented()});
  frames(segmented, 3);
  EXPECT_EQ(frame_of(tabs, "picker").height,
            frame_of(segmented, "picker").height);
  press(providers, tabs, provider::Button::dpad_right);
  EXPECT_TRUE(shows(tabs, "Green"));
  EXPECT_EQ(controls::PickerStyle::tabs(),
            controls::PickerStyle{controls::TabsPickerStyle{}});
}

TEST_F(TabParityTest, OnScrollUpMinimizesWhenScrollingBack) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers, TabView(Tab("Feed", "home", 0, long_page()),
                         Tab("Two", "star", 1, page("two")))
                     .tab_bar_minimize_behavior(
                         structure::TabBarMinimizeBehavior::on_scroll_up));
  frames(application, 4);
  drag(providers, application, {.x = 640.0F, .y = 600.0F},
       {.x = 640.0F, .y = 200.0F}, 8);
  settle(providers, application);
  EXPECT_TRUE(node_of(application, key(1)).has_value());
  drag(providers, application, {.x = 640.0F, .y = 300.0F},
       {.x = 640.0F, .y = 420.0F}, 6);
  settle(providers, application);
  EXPECT_FALSE(node_of(application, key(1)).has_value());
}

TEST_F(TabParityTest, TabBarPlacementCasesAreDistinct) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers, TabView(Tab("One", "home", 0, page("paged one")),
                               Tab("Two", "star", 1, page("paged two")))
                           .tab_view_style(structure::PageTabViewStyle{}));
  frames(application, 3);
  EXPECT_EQ(seen().placement, std::optional<structure::TabBarPlacement>{
                                  structure::TabBarPlacement::page_indicator});
  EXPECT_NE(structure::TabBarPlacement::bottom_bar,
            structure::TabBarPlacement::ornament);
  EXPECT_NE(structure::TabBarPlacement::bottom_bar,
            structure::TabBarPlacement::top_bar);
}

TEST_F(TabParityTest, SidebarSectionsCollapseSwipeAndCarryHeaders) {
  provider::headless::HeadlessProviderFactory providers;
  std::vector<Element> library;
  library.push_back(
      Tab("Songs", "music", 1, page("songs"))
          .swipe_actions(composition::HorizontalEdge::trailing, true, [] {
            return Element{components::Button("Unpin", [] {})};
          }));
  library.push_back(Tab("Albums", "album", 2, page("albums")));
  std::vector<Element> hidden;
  hidden.push_back(Tab("Radio", "radio", 3, page("radio")));
  Application application =
      start(providers,
            TabView(Tab("Home", "home", 0, page("home")),
                    structure::TabSection(std::move(library),
                                          components::Text("Custom header"))
                        .customization_id("library")
                        .default_section_expansion(
                            structure::TabSectionExpansion::expanded),
                    structure::TabSection("Later", std::move(hidden))
                        .default_section_expansion(
                            structure::TabSectionExpansion::collapsed))
                .tab_view_style(structure::SidebarAdaptableTabViewStyle{}));
  frames(application, 4);
  EXPECT_TRUE(shows(application, "Custom header"));
  EXPECT_TRUE(shows(application, "Songs"));
  EXPECT_FALSE(shows(application, "Radio"));
  tap(providers, application, centre(application, "arisa.tab.section.1"));
  frames(application, 30);
  EXPECT_TRUE(shows(application, "Radio"));
  tap(providers, application, centre(application, "arisa.tab.section.0"));
  frames(application, 30);
  EXPECT_FALSE(shows(application, "Songs"));
  tap(providers, application, centre(application, "arisa.tab.section.0"));
  frames(application, 30);
  const provider::PixelPoint from = centre(application, key(1));
  drag(providers, application, from,
       provider::PixelPoint{.x = from.x - 60.0F, .y = from.y}, 12);
  frames(application, 30);
  EXPECT_TRUE(shows(application, "Unpin"));
}

TEST_F(TabParityTest, TopBarItemsAtTheWindowRootFitTheWindow) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers, TabView(Tab("Home", "home", 0, page("home")),
                               Tab("Two", "star", 1, page("two")),
                               Tab("Three", "star", 2, page("three"))));
  frames(application, 4);
  const composition::Rect bar = frame_of(application, "arisa.tab.bar");
  ASSERT_GT(bar.width, 0.0F);
  float right = 0.0F;
  for (std::size_t index = 0; index < 3; ++index) {
    const composition::Rect item = frame_of(application, key(index));
    EXPECT_GE(item.x, 0.0F) << index;
    EXPECT_LE(item.x + item.width, 1280.0F) << index;
    EXPECT_GE(item.x, right) << index;
    right = item.x + item.width;
  }
  EXPECT_LT(bar.width, 1280.0F);
}

}
