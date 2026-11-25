#include <arisa/components/button.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/safe_area_inset.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/user_interface_size_class.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/navigation_destination.hpp>
#include <arisa/composition/navigation_path.hpp>
#include <arisa/composition/navigation_split_view_style.hpp>
#include <arisa/composition/navigation_transition.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/toolbar_item.hpp>
#include <arisa/controls/searchable.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/with_animation.hpp>
#include <arisa/presentation/navigation_stack.hpp>
#include <arisa/presentation/presented_destination.hpp>
#include <arisa/presentation/toolbar_layout.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/structure/navigation_split_view.hpp>
#include <arisa/structure/scaffold.hpp>
#include <arisa/structure/tab.hpp>
#include <arisa/structure/tab_view.hpp>
#include <arisa/structure/tab_view_customization.hpp>
#include <arisa/structure/tab_view_style.hpp>
#include <arisa/theme/shape.hpp>

#include <scroll_fixture.hpp>

#include <gtest/gtest.h>

#include <format>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace arisa::runtime {

using namespace std::chrono_literals;
using composition::Element;
using composition::infinity;
using composition::ToolbarItem;
using composition::ToolbarItemPlacement;
using provider::Button;

class NavigationChromeTest : public ScrollFixture {
 protected:
  struct Probe {
    std::optional<composition::StateHandle<bool>> presented;
    std::optional<composition::StateHandle<std::optional<int>>> item;
    std::optional<composition::StateHandle<std::vector<int>>> path;
  };

  class Destinations final : public composition::Component {
   public:
    static constexpr composition::StateKey<bool> shown{"chrome-shown", false};
    static constexpr composition::StateKey<std::optional<int>> chosen{
        "chrome-chosen", std::nullopt};

    explicit Destinations(Probe* probe) noexcept : probe_(probe) {}

    auto operator==(const Destinations&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const auto flag = context.state(shown);
      const auto item = context.state(chosen);
      probe_->presented = flag;
      probe_->item = item;
      return presentation::NavigationStack([flag, item] {
        return components::VStack(
                   components::Button("open", [flag] { flag.set(true); })
                       .frame({.width = 120.0F, .height = 40.0F})
                       .id("open"))
            .navigation_title("Root")
            .navigation_destination(
                composition::Binding<bool>{flag},
                [] { return components::Text("flagged page"); })
            .navigation_destination(item, [](const int& value) {
              return components::Text(std::format("item {}", value));
            });
      });
    }

   private:
    Probe* probe_;
  };

  class BoundPath final : public composition::Component {
   public:
    static const composition::StateKey<std::vector<int>> backing;

    explicit BoundPath(Probe* probe) noexcept : probe_(probe) {}

    auto operator==(const BoundPath&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const auto stored = context.state(backing);
      probe_->path = stored;
      const composition::Binding<std::vector<int>> pages{
          [stored] { return stored.get(); },
          [stored](std::vector<int> value) { stored.set(std::move(value)); }};
      return presentation::NavigationStack(pages, [] {
        return components::Text("bound root")
            .navigation_destination(
                composition::for_type<int>, [](const int& value) {
                  return components::Text(std::format("bound {}", value));
                });
      });
    }

   private:
    Probe* probe_;
  };

  class Locked final : public composition::Component {
   public:
    static const composition::StateKey<std::vector<int>> pages;

    explicit Locked(Probe* probe) noexcept : probe_(probe) {}

    auto operator==(const Locked&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const auto path = context.state(pages);
      probe_->path = path;
      return presentation::NavigationStack(path, [] {
        return components::Text("root page")
            .navigation_destination(composition::for_type<int>, [](const int&) {
              return components::Text("locked page")
                  .navigation_title("Locked")
                  .navigation_subtitle("Read only")
                  .navigation_bar_back_button_hidden();
            });
      });
    }

   private:
    Probe* probe_;
  };

  class Zoomed final : public composition::Component {
   public:
    static constexpr composition::Namespace space{"chrome-zoom"};
    static const composition::StateKey<std::vector<int>> pages;

    explicit Zoomed(Probe* probe) noexcept : probe_(probe) {}

    auto operator==(const Zoomed&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const auto path = context.state(pages);
      probe_->path = path;
      const composition::Namespace::ID ns = context.namespace_id(space);
      return presentation::NavigationStack(path, [ns] {
        return components::VStack(
                   components::ZStack()
                       .frame({.width = 80.0F, .height = 60.0F})
                       .background(theme::Colour::hex(0x3366CC),
                                   theme::RoundedRectangle(12.0F))
                       .matched_transition_source(1, ns)
                       .id("card"))
            .navigation_destination(
                composition::for_type<int>, [ns](const int& value) {
                  return components::Text(std::format("detail {}", value))
                      .frame({.max_width = infinity, .max_height = infinity})
                      .navigation_transition(
                          composition::NavigationTransition::zoom(value, ns));
                });
      });
    }

   private:
    Probe* probe_;
  };

  static auto screen_of(Application& application, std::string_view text)
      -> std::optional<composition::Rect> {
    std::optional<composition::Rect> found;
    const auto search = [&](this const auto& self,
                            const composition::Node& node,
                            const composition::Node* screen) -> void {
      const composition::Node* holder = screen;
      if (node.zoom_source().has_value()) {
        holder = &node;
      }
      if (const auto written = node.content_as<components::Text::Content>();
          written.has_value() && written->get().text() == text &&
          holder != nullptr && holder->presented().has_value()) {
        found = holder->presented()->bounds;
      }
      for (const composition::Node& child : node.children()) {
        self(child, holder);
      }
    };
    search(application.root().tree().root(), nullptr);
    return found;
  }

  static auto single(std::function<Element()> root) -> Element {
    return presentation::NavigationStack(
        [root = std::move(root)] { return root(); });
  }
};

const composition::StateKey<std::vector<int>>
    NavigationChromeTest::BoundPath::backing{"chrome-backing", {}};

const composition::StateKey<std::vector<int>>
    NavigationChromeTest::Locked::pages{"chrome-locked", {}};

const composition::StateKey<std::vector<int>>
    NavigationChromeTest::Zoomed::pages{"chrome-zoomed", {}};

TEST_F(NavigationChromeTest, ZoomTransitionGrowsFromTheSourceAndShrinksBack) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  Application application = start(providers, Zoomed{&probe});
  const composition::Rect card = node_named(application, "card")->get().frame();
  motion::with_animation(motion::Animation::linear(1.0),
                         [&probe] { probe.path->set({1}); });
  frames(application, 1);
  const auto opening = screen_of(application, "detail 1");
  ASSERT_TRUE(opening.has_value());
  EXPECT_NEAR(opening->width, card.width, 2.0F);
  EXPECT_NEAR(opening->x, card.x, 2.0F);
  ASSERT_TRUE(
      providers.clock().advance(std::chrono::milliseconds{500}).has_value());
  frames(application, 1);
  const auto halfway = screen_of(application, "detail 1");
  ASSERT_TRUE(halfway.has_value());
  EXPECT_GT(halfway->width, card.width + 50.0F);
  settle(providers, application);
  const auto open = screen_of(application, "detail 1");
  ASSERT_TRUE(open.has_value());
  EXPECT_GT(open->width, 500.0F);
  motion::with_animation(motion::Animation::linear(1.0),
                         [&probe] { probe.path->set({}); });
  frames(application, 1);
  ASSERT_TRUE(
      providers.clock().advance(std::chrono::milliseconds{500}).has_value());
  frames(application, 1);
  const auto closing = screen_of(application, "detail 1");
  ASSERT_TRUE(closing.has_value());
  EXPECT_LT(closing->width, open->width - 50.0F);
  settle(providers, application);
  EXPECT_FALSE(shows(application, "detail 1"));
}

TEST_F(NavigationChromeTest, BooleanDestinationPushesAndBackClearsTheFlag) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  Application application = start(providers, Destinations{&probe});
  probe.presented->set(true);
  frames(application, 4);
  EXPECT_TRUE(shows(application, "flagged page"));
  press(providers, application, Button::b);
  frames(application, 2);
  settle(providers, application);
  EXPECT_FALSE(probe.presented->get());
  EXPECT_FALSE(shows(application, "flagged page"));
}

TEST_F(NavigationChromeTest, ItemDestinationReplacesAndClears) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  Application application = start(providers, Destinations{&probe});
  probe.item->set(4);
  frames(application, 4);
  EXPECT_TRUE(shows(application, "item 4"));
  probe.item->set(7);
  frames(application, 4);
  settle(providers, application);
  EXPECT_TRUE(shows(application, "item 7"));
  EXPECT_FALSE(shows(application, "item 4"));
  EXPECT_EQ(probe.item->get(), std::optional<int>{7});
  probe.item->set(std::nullopt);
  frames(application, 4);
  settle(providers, application);
  EXPECT_FALSE(shows(application, "item 7"));
}

TEST_F(NavigationChromeTest, PathBindingDrivesTheStack) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  Application application = start(providers, BoundPath{&probe});
  probe.path->set({1, 2});
  frames(application, 4);
  EXPECT_TRUE(shows(application, "bound 2"));
  press(providers, application, Button::b);
  frames(application, 2);
  EXPECT_EQ(probe.path->get(), std::vector<int>{1});
}

TEST_F(NavigationChromeTest, SubtitleAndHiddenBackButton) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  Application application = start(providers, Locked{&probe});
  probe.path->set({1});
  frames(application, 4);
  settle(providers, application);
  EXPECT_TRUE(shows(application, "Locked"));
  EXPECT_TRUE(shows(application, "Read only"));
  EXPECT_FALSE(shows(application, "\u2039 Back"));
  press(providers, application, Button::b);
  frames(application, 2);
  EXPECT_EQ(probe.path->get(), std::vector<int>{1});
}

TEST_F(NavigationChromeTest, ToolbarGroupsSpacersOverflowAndTitleMenu) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers, single([] {
        return Element{
            components::Text("body")
                .navigation_title("Edit")
                .navigation_subtitle("Draft")
                .toolbar_title_menu([] {
                  return components::Group(components::Button("Rename", [] {}));
                })
                .toolbar(
                    composition::ToolbarItemGroup(
                        ToolbarItemPlacement::primary_action,
                        components::Button("one", [] {}).id("one"),
                        components::Button("two", [] {}).id("two")),
                    composition::ToolbarSpacer(
                        composition::SpacerSizing::fixed,
                        ToolbarItemPlacement::primary_action),
                    ToolbarItem(ToolbarItemPlacement::primary_action,
                                components::Button("three", [] {}).id("three")),
                    ToolbarItem(
                        ToolbarItemPlacement::secondary_action,
                        [] { return components::Button("hidden", [] {}); }),
                    ToolbarItem(
                        ToolbarItemPlacement::bottom_bar,
                        components::Button("below", [] {}).id("below")))};
      }));
  frames(application, 3);
  EXPECT_TRUE(shows(application, "Edit"));
  EXPECT_TRUE(shows(application, "Draft"));
  EXPECT_FALSE(shows(application, "hidden"));
  EXPECT_TRUE(node_of(application, presentation::ToolbarLayout::overflow_key)
                  .has_value());
  EXPECT_TRUE(node_of(application, presentation::ToolbarLayout::title_menu_key)
                  .has_value());
  const composition::Rect two = frame_of(application, "two");
  const composition::Rect three = frame_of(application, "three");
  EXPECT_GE(three.x - (two.x + two.width),
            theme::metrics::navigation.group_gap);
  EXPECT_TRUE(node_of(application, presentation::NavigationStack::bottom_bar_id)
                  .has_value());
  EXPECT_GT(frame_of(application, "below").y, frame_of(application, "one").y);
  tap(providers, application,
      centre(application, presentation::ToolbarLayout::overflow_key));
  frames(application, 3);
  EXPECT_TRUE(shows(application, "hidden"));
}

TEST_F(NavigationChromeTest, ToolbarBackgroundSchemeAndForegroundStyleTheBar) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers, single([] {
        return Element{
            components::Text("body")
                .toolbar(composition::ToolbarItem(
                    composition::ToolbarItemPlacement::bottom_bar,
                    components::Button("below", [] {}).id("below")))
                .toolbar_background(theme::ColourRole::red,
                                    composition::ToolbarPlacement::bottom_bar)
                .toolbar_color_scheme(theme::ColorScheme::dark,
                                      composition::ToolbarPlacement::bottom_bar)
                .toolbar_foreground_style(
                    theme::ColourRole::orange,
                    composition::ToolbarPlacement::bottom_bar)};
      }));
  frames(application, 3);
  const auto bar =
      node_of(application, presentation::NavigationStack::bottom_bar_id);
  ASSERT_TRUE(bar.has_value());
  const composition::Node& node = application.root().tree().node(*bar)->get();
  const auto scheme = node.override_of(components::Environment::colour_scheme);
  ASSERT_TRUE(scheme.has_value());
  EXPECT_EQ(scheme->get(), std::optional{theme::ColorScheme::dark});
  ASSERT_TRUE(node.modifiers().background.has_value());
  EXPECT_EQ(node.modifiers().background->fill.colour(),
            std::optional{theme::ColourValue{theme::ColourRole::red}});
  EXPECT_TRUE(node.modifiers().foreground.has_value());
}

TEST_F(NavigationChromeTest, AHiddenToolbarBackgroundDropsTheBarFill) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers, single([] {
        return Element{
            components::Text("body")
                .toolbar(composition::ToolbarItem(
                    composition::ToolbarItemPlacement::bottom_bar,
                    components::Button("below", [] {}).id("below")))
                .toolbar_background(theme::ColourRole::red,
                                    composition::ToolbarPlacement::bottom_bar)
                .toolbar_background(composition::Visibility::hidden,
                                    composition::ToolbarPlacement::bottom_bar)};
      }));
  frames(application, 3);
  const auto bar =
      node_of(application, presentation::NavigationStack::bottom_bar_id);
  ASSERT_TRUE(bar.has_value());
  EXPECT_FALSE(application.root()
                   .tree()
                   .node(*bar)
                   ->get()
                   .modifiers()
                   .background.has_value());
}

TEST_F(NavigationChromeTest, HiddenNavigationBarDropsTheTitle) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers, single([] {
        return Element{components::Text("body")
                           .navigation_title("Gone")
                           .toolbar_visibility(
                               composition::Visibility::hidden,
                               composition::ToolbarPlacement::navigation_bar)};
      }));
  frames(application, 3);
  EXPECT_FALSE(shows(application, "Gone"));
  EXPECT_TRUE(shows(application, "body"));
}

TEST_F(NavigationChromeTest, NavigationStackRoleCentresTheTitle) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers, single([] {
        return Element{
            components::Text("body")
                .toolbar(ToolbarItem(ToolbarItemPlacement::principal,
                                     components::Text("Middle").id("middle")))
                .toolbar_role(composition::ToolbarRole::navigation_stack)};
      }));
  frames(application, 3);
  const composition::Rect middle = frame_of(application, "middle");
  const composition::Rect root = application.root().tree().root().frame();
  EXPECT_NEAR(middle.x + (middle.width * 0.5F), root.width * 0.5F, 2.0F);
}

TEST_F(NavigationChromeTest, LargeTitleCollapsesOnScroll) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers, single([] {
              std::vector<Element> rows;
              for (int index = 0; index < 40; ++index) {
                rows.push_back(components::Text(std::format("row {}", index))
                                   .frame({.height = 40.0F}));
              }
              return Element{
                  components::ScrollView(components::VStack(std::move(rows)))
                      .frame({.max_width = infinity, .max_height = infinity})
                      .id("scroller")
                      .navigation_title("Big")
                      .navigation_bar_title_display_mode(
                          composition::NavigationBarTitleDisplayMode::large)};
            }));
  frames(application, 3);
  EXPECT_TRUE(
      node_of(application, presentation::NavigationStack::large_title_id)
          .has_value());
  const provider::PixelPoint middle = centre(application, "scroller");
  drag(providers, application, middle,
       provider::PixelPoint{.x = middle.x, .y = middle.y - 200.0F}, 8);
  settle(providers, application);
  const auto kept =
      node_of(application, presentation::NavigationStack::large_title_id);
  ASSERT_TRUE(kept.has_value());
  EXPECT_LE(application.root().tree().node(*kept)->get().modifiers().opacity,
            0.0F);
  EXPECT_TRUE(shows(application, "Big"));
}

TEST_F(NavigationChromeTest, LargeTitleScrollsInsideTheContentMargins) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers, single([] {
        std::vector<Element> rows;
        for (int index = 0; index < 40; ++index) {
          Element row = components::Text(std::format("row {}", index))
                            .frame({.height = 40.0F});
          if (index == 0) {
            row = std::move(row).id("first");
          }
          rows.push_back(std::move(row));
        }
        return Element{
            components::ScrollView(
                components::VStack(
                    components::VStackOptions{.spacing = theme::space::none},
                    std::move(rows)))
                .frame({.max_width = infinity, .max_height = infinity})
                .id("scroller")
                .navigation_title("Big")
                .navigation_bar_title_display_mode(
                    composition::NavigationBarTitleDisplayMode::large)};
      }));
  frames(application, 3);
  const composition::Tree& tree = application.root().tree();
  const auto title =
      node_of(application, presentation::NavigationStack::large_title_id);
  const auto scroller = node_of(application, "scroller");
  ASSERT_TRUE(title.has_value());
  ASSERT_TRUE(scroller.has_value());
  const composition::Rect heading = tree.node(*title)->get().frame();
  const composition::Rect view = tree.node(*scroller)->get().frame();
  EXPECT_LE(view.y, heading.y + 0.5F);
  EXPECT_NEAR(tree.node(*scroller)->get().content_insets().top,
              heading.y + heading.height - view.y, 0.5F);
  const auto first = [&application] {
    return presented_of(application, "first").y;
  };
  const float resting = first();
  EXPECT_NEAR(resting, heading.y + heading.height, 0.5F);
  const provider::PixelPoint middle = centre(application, "scroller");
  drag(providers, application, middle,
       provider::PixelPoint{.x = middle.x, .y = middle.y - 400.0F}, 8);
  settle(providers, application);
  EXPECT_LT(first(), view.y);
  EXPECT_NEAR(tree.node(*scroller)->get().content_insets().top,
              heading.y + heading.height - view.y, 0.5F);
}

TEST_F(NavigationChromeTest, LargeTitleTravelsWithTheContentBeforeCollapsing) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers, single([] {
              std::vector<Element> rows;
              for (int index = 0; index < 40; ++index) {
                rows.push_back(components::Text(std::format("row {}", index))
                                   .frame({.height = 40.0F}));
              }
              return Element{
                  components::ScrollView(components::VStack(std::move(rows)))
                      .frame({.max_width = infinity, .max_height = infinity})
                      .id("scroller")
                      .navigation_title("Big")
                      .navigation_bar_title_display_mode(
                          composition::NavigationBarTitleDisplayMode::large)};
            }));
  frames(application, 3);
  const auto heading = [&application]() -> std::optional<composition::Rect> {
    const auto id =
        node_of(application, presentation::NavigationStack::large_title_id);
    if (!id.has_value()) {
      return std::nullopt;
    }
    const composition::Node* at = &application.root().tree().node(*id)->get();
    while (at->child_count() == 1) {
      at = &*at->children().begin();
    }
    return at->presented().has_value() ? std::optional{at->presented()->bounds}
                                       : std::nullopt;
  };
  const auto resting = heading();
  ASSERT_TRUE(resting.has_value());
  const provider::PixelPoint middle = centre(application, "scroller");
  drag(providers, application, middle,
       provider::PixelPoint{.x = middle.x, .y = middle.y - 20.0F}, 4, false);
  frames(application, 2);
  const auto travelled = heading();
  ASSERT_TRUE(travelled.has_value());
  EXPECT_LT(travelled->y, resting->y - 8.0F);
}

TEST_F(NavigationChromeTest, SafeAreaInsetShrinksContentBySpacingAndBar) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers,
            components::ZStack(
                components::Text("content")
                    .frame({.max_width = infinity, .max_height = infinity})
                    .id("content")
                    .safe_area_inset(
                        composition::VerticalEdge::bottom,
                        components::ZStack()
                            .frame({.height = 40.0F, .max_width = infinity})
                            .id("bar")))
                .frame({.width = 200.0F, .height = 300.0F}));
  frames(application, 3);
  const composition::Rect content = frame_of(application, "content");
  const composition::Rect bar = frame_of(application, "bar");
  EXPECT_FLOAT_EQ(content.height, 252.0F);
  EXPECT_FLOAT_EQ(bar.y - (content.y + content.height),
                  components::SafeAreaInset::inset_spacing);
  EXPECT_FLOAT_EQ(bar.width, 200.0F);
}

TEST_F(NavigationChromeTest, SafeAreaBarInsetsAScrollView) {
  provider::headless::HeadlessProviderFactory providers;
  std::vector<Element> rows;
  for (int index = 0; index < 20; ++index) {
    rows.push_back(components::ZStack()
                       .frame({.height = 30.0F})
                       .id(std::format("row-{}", index)));
  }
  Application application = start(
      providers,
      components::VStack(
          components::ScrollView(
              components::VStack(
                  components::VStackOptions{.spacing = theme::space::none},
                  std::move(rows)))
              .frame({.max_width = infinity, .max_height = infinity})
              .id("scroll")
              .safe_area_bar(
                  composition::VerticalEdge::top,
                  components::ZStack().frame({.height = 40.0F}).id("bar")))
          .frame({.width = 200.0F, .height = 300.0F}));
  frames(application, 3);
  const auto scroll = node_named(application, "scroll");
  ASSERT_TRUE(scroll.has_value());
  EXPECT_FLOAT_EQ(scroll->get().safe_area().top, 40.0F);
  EXPECT_FLOAT_EQ(frame_of(application, "scroll").height, 300.0F);
}

TEST_F(NavigationChromeTest, ThreeColumnSplitCollapsesWhenCompact) {
  provider::headless::HeadlessProviderFactory providers;
  const auto split = [](components::UserInterfaceSizeClass size) {
    return Element{
        structure::NavigationSplitView(
            components::ZStack()
                .frame({.max_width = infinity, .max_height = infinity})
                .id("sidebar"),
            components::ZStack()
                .frame({.max_width = infinity, .max_height = infinity})
                .id("content"),
            components::Button("detail", [] {})
                .frame({.max_width = infinity, .max_height = infinity})
                .id("detail"))}
        .environment(components::Environment::horizontal_size_class, size);
  };
  Application regular =
      start(providers, split(components::UserInterfaceSizeClass::regular));
  frames(regular, 3);
  const composition::Rect sidebar = frame_of(regular, "sidebar");
  const composition::Rect content = frame_of(regular, "content");
  const composition::Rect detail = frame_of(regular, "detail");
  EXPECT_FLOAT_EQ(content.width, structure::NavigationSplitView::content_width);
  EXPECT_NEAR(content.x, sidebar.x + sidebar.width, 1.0F);
  EXPECT_NEAR(detail.x, content.x + content.width, 1.0F);
  provider::headless::HeadlessProviderFactory handheld;
  Application compact =
      start(handheld, split(components::UserInterfaceSizeClass::compact));
  frames(compact, 3);
  EXPECT_NEAR(frame_of(compact, "content").x, 0.0F, 1.0F);
  EXPECT_GT(frame_of(compact, "detail").x, 0.0F);
  press(handheld, compact, structure::NavigationSplitView::toggle_button);
  settle(handheld, compact);
  EXPECT_NEAR(frame_of(compact, "detail").x, 0.0F, 1.0F);
}

TEST_F(NavigationChromeTest, SplitViewStylesPlaceTheDetailColumn) {
  const auto split = [](composition::NavigationSplitViewStyle style) {
    return Element{
        structure::NavigationSplitView(
            components::ZStack()
                .frame({.max_width = infinity, .max_height = infinity})
                .id("sidebar"),
            components::ZStack()
                .frame({.max_width = infinity, .max_height = infinity})
                .id("content"),
            components::Button("detail", [] {})
                .frame({.max_width = infinity, .max_height = infinity})
                .id("detail"))
            .navigation_split_view_style(style)}
        .environment(components::Environment::horizontal_size_class,
                     components::UserInterfaceSizeClass::regular);
  };
  provider::headless::HeadlessProviderFactory balanced_providers;
  Application balanced =
      start(balanced_providers,
            split(composition::NavigationSplitViewStyle::balanced));
  frames(balanced, 3);
  const composition::Rect root = balanced.root().tree().root().frame();
  const composition::Rect beside = frame_of(balanced, "detail");
  EXPECT_GT(beside.x, structure::NavigationSplitView::content_width);
  EXPECT_LT(beside.width,
            root.width - structure::NavigationSplitView::content_width + 1.0F);
  provider::headless::HeadlessProviderFactory prominent_providers;
  Application prominent =
      start(prominent_providers,
            split(composition::NavigationSplitViewStyle::prominent_detail));
  frames(prominent, 3);
  const composition::Rect wide = frame_of(prominent, "detail");
  EXPECT_NEAR(wide.x, 0.0F, 1.0F);
  EXPECT_GT(wide.width, beside.width);
}

TEST_F(NavigationChromeTest, ColumnWidthAndColumnContainer) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers,
      Element{structure::NavigationSplitView(
                  components::ZStack()
                      .frame({.max_width = infinity, .max_height = infinity})
                      .navigation_split_view_column_width(180.0F)
                      .id("sidebar"),
                  components::ZStack()
                      .frame({.max_width = infinity, .max_height = infinity})
                      .navigation_split_view_column_width(std::nullopt, 400.0F,
                                                          260.0F),
                  components::VStack(components::ZStack()
                                         .container_relative_frame(
                                             composition::Axes::horizontal)
                                         .frame({.height = 10.0F})
                                         .id("half")))}
          .environment(components::Environment::horizontal_size_class,
                       components::UserInterfaceSizeClass::regular));
  frames(application, 5);
  EXPECT_FLOAT_EQ(frame_of(application, "sidebar").width, 180.0F);
  const composition::Rect root = application.root().tree().root().frame();
  EXPECT_NEAR(frame_of(application, "half").width, root.width - 180.0F - 260.0F,
              1.0F);
}

TEST_F(NavigationChromeTest, PagedTabViewStepsAndShowsDots) {
  provider::headless::HeadlessProviderFactory providers;
  static constexpr composition::StateKey<int> step{"paged-step", 0};
  class Paged final : public composition::Component {
   public:
    auto operator==(const Paged&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const auto selected = context.state(step);
      return Element{
          structure::TabView(
              selected,
              structure::Tab(
                  "A", "circle", 0,
                  [] {
                    return components::Button("page a", [] {}).id("page-a");
                  }),
              structure::Tab(
                  "B", "circle", 1,
                  [] {
                    return components::Button("page b", [] {}).id("page-b");
                  }),
              structure::Tab(
                  "C", "circle", 2,
                  [] {
                    return components::Button("page c", [] {}).id("page-c");
                  }))}
          .tab_view_style(structure::PageTabViewStyle{});
    }
  };
  Application application = start(providers, Paged{});
  frames(application, 3);
  EXPECT_EQ(focused_key(application), "page-a");
  EXPECT_FALSE(node_of(application, "page-c").has_value());
  EXPECT_TRUE(node_of(application, structure::TabView::dots_id).has_value());
  press(providers, application, Button::r);
  settle(providers, application);
  EXPECT_EQ(focused_key(application), "page-b");
  press(providers, application, Button::r);
  press(providers, application, Button::r);
  settle(providers, application);
  EXPECT_EQ(focused_key(application), "page-c");
  EXPECT_NEAR(frame_of(application, "page-c").x,
              frame_of(application, structure::TabView::page_id).x, 400.0F);
  press(providers, application, Button::dpad_left);
  settle(providers, application);
  EXPECT_EQ(focused_key(application), "page-b");
}

TEST_F(NavigationChromeTest, PagedTabViewWithNothingFocusableStillTurns) {
  provider::headless::HeadlessProviderFactory providers;
  static constexpr composition::StateKey<int> step{"plain-step", 0};
  class Plain final : public composition::Component {
   public:
    auto operator==(const Plain&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const auto selected = context.state(step);
      return Element{
          structure::TabView(
              selected,
              structure::Tab(
                  "A", "circle", 0,
                  [] { return components::Text("page a").id("text-a"); }),
              structure::Tab(
                  "B", "circle", 1,
                  [] { return components::Text("page b").id("text-b"); }))}
          .tab_view_style(structure::PageTabViewStyle{});
    }
  };
  Application application = start(providers, Plain{});
  frames(application, 3);
  const float home = frame_of(application, structure::TabView::page_id).x;
  press(providers, application, Button::r);
  settle(providers, application);
  EXPECT_NEAR(frame_of(application, "text-b").x, home, 1.0F);
  press(providers, application, Button::dpad_left);
  settle(providers, application);
  EXPECT_NEAR(frame_of(application, "text-a").x, home, 1.0F);
}

TEST_F(NavigationChromeTest, TabSectionsBadgesAndSearch) {
  provider::headless::HeadlessProviderFactory providers;
  const auto page = [](std::string text) {
    return structure::Tab::Builder{[text] { return components::Text(text); }};
  };
  Application application = start(
      providers,
      Element{structure::TabView(
                  structure::TabSection(
                      "Library",
                      Element{structure::Tab("Games", "circle", 0,
                                             page("games page"))}
                          .badge(3),
                      structure::Tab("Media", "circle", 1, page("media page"))),
                  structure::Tab(structure::TabRole::search, 2,
                                 page("search page")))}
          .tab_view_style(structure::SidebarAdaptableTabViewStyle{}));
  frames(application, 3);
  EXPECT_TRUE(shows(application, "Library"));
  EXPECT_TRUE(shows(application, "3"));
  EXPECT_TRUE(shows(application, "Search"));
  EXPECT_LT(
      frame_of(application, structure::TabView::item_key(2).description()).y,
      frame_of(application, structure::TabView::item_key(0).description()).y);
}

TEST_F(NavigationChromeTest, CustomizationRoundTripsAndOrders) {
  structure::TabViewCustomization customization;
  customization.reorder({"b", "a"});
  customization.set_sidebar_visibility("c", composition::Visibility::hidden);
  const std::optional<structure::TabViewCustomization> decoded =
      structure::TabViewCustomization::decoded(customization.encoded());
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(*decoded, customization);
  const std::vector<std::string> ids{"a", "", "b", "c"};
  EXPECT_EQ(customization.arranged(ids), (std::vector<std::size_t>{2, 1, 0}));
}

TEST_F(NavigationChromeTest, ScaffoldPlacesBottomBarItemsInTheFooter) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers,
      structure::Scaffold(
          components::Button("content", [] {})
              .id("content")
              .navigation_title("Home")
              .navigation_subtitle("Today")
              .toolbar(
                  ToolbarItem(ToolbarItemPlacement::bottom_bar,
                              components::Button("below", [] {}).id("below")),
                  ToolbarItem(ToolbarItemPlacement::secondary_action, [] {
                    return components::Button("more", [] {});
                  }))));
  frames(application, 4);
  EXPECT_TRUE(shows(application, "Today"));
  EXPECT_TRUE(node_of(application, "below").has_value());
  EXPECT_GT(frame_of(application, "below").y,
            frame_of(application, "content").y);
  EXPECT_TRUE(node_of(application, presentation::ToolbarLayout::overflow_key)
                  .has_value());
}

TEST_F(NavigationChromeTest, ToolbarSearchFieldSitsInTheBar) {
  provider::headless::HeadlessProviderFactory providers;
  static constexpr composition::StateKey<std::string> query{"chrome-query", ""};
  class Searching final : public composition::Component {
   public:
    auto operator==(const Searching&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const auto text = context.state(query);
      return presentation::NavigationStack([text] {
        return components::Text("results")
            .id("results")
            .searchable(text, composition::SearchFieldPlacement::toolbar)
            .navigation_title("Find");
      });
    }
  };
  Application application = start(providers, Searching{});
  frames(application, 4);
  ASSERT_TRUE(node_of(application, "arisa.search.bar").has_value());
  EXPECT_LT(frame_of(application, "arisa.search.bar").y,
            frame_of(application, "results").y);
  EXPECT_NEAR(frame_of(application, "arisa.search.bar").width,
              controls::Searchable::toolbar_width, 1.0F);
}

TEST_F(NavigationChromeTest, SidebarTabsReorderIntoTheCustomization) {
  provider::headless::HeadlessProviderFactory providers;
  static const composition::StateKey<structure::TabViewCustomization> order{
      "chrome-order", structure::TabViewCustomization{}};
  struct Holder {
    std::optional<composition::StateHandle<structure::TabViewCustomization>>
        handle;
  };
  static Holder holder;
  class Reordering final : public composition::Component {
   public:
    auto operator==(const Reordering&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const auto saved = context.state(order);
      holder.handle = saved;
      const auto page = [](std::string text) {
        return structure::Tab::Builder{
            [text] { return components::Text(text); }};
      };
      return Element{
          structure::TabView(
              structure::Tab("One", "circle", 0, page("one page"))
                  .customization_id("one"),
              structure::Tab("Two", "circle", 1, page("two page"))
                  .customization_id("two"),
              structure::Tab("Three", "circle", 2, page("three page"))
                  .customization_id("three"))
              .tab_view_customization(saved)}
          .tab_view_style(structure::SidebarAdaptableTabViewStyle{});
    }
  };
  Application application = start(providers, Reordering{});
  frames(application, 3);
  EXPECT_EQ(focused_key(application),
            structure::TabView::item_key(0).description());
  press(providers, application, Button::x);
  press(providers, application, Button::dpad_down);
  press(providers, application, Button::a);
  settle(providers, application);
  const std::vector<std::string> expected{"two", "one", "three"};
  const std::span<const std::string> stored = holder.handle->get().order();
  EXPECT_EQ(std::vector<std::string>(stored.begin(), stored.end()), expected);
  EXPECT_GT(
      frame_of(application, structure::TabView::item_key(0).description()).y,
      frame_of(application, structure::TabView::item_key(1).description()).y);
}

TEST_F(NavigationChromeTest, TabContentCanHideTheTabBar) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers,
      Element{structure::TabView(
          structure::Tab("One", "circle", 0,
                         [] {
                           return components::Button("first", [] {})
                               .id("first")
                               .toolbar_visibility(
                                   composition::Visibility::hidden,
                                   composition::ToolbarPlacement::tab_bar);
                         }),
          structure::Tab("Two", "circle", 1,
                         [] { return components::Text("second"); }))});
  frames(application, 4);
  EXPECT_FALSE(node_of(application, "arisa.tab.bar").has_value());
  EXPECT_TRUE(node_of(application, "first").has_value());
}

}
