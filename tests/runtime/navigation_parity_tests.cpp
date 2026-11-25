#include <arisa/components/button.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/user_interface_size_class.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/alignment.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/navigation_destination.hpp>
#include <arisa/composition/navigation_path.hpp>
#include <arisa/composition/navigation_split_view_column.hpp>
#include <arisa/composition/navigation_transition.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/toolbar_placement.hpp>
#include <arisa/composition/transferable.hpp>
#include <arisa/presentation/navigation_link.hpp>
#include <arisa/presentation/navigation_stack.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/structure/form.hpp>
#include <arisa/structure/navigation_split_view.hpp>
#include <arisa/structure/navigation_view.hpp>
#include <arisa/structure/tab.hpp>
#include <arisa/structure/tab_view.hpp>
#include <arisa/theme/colour_value.hpp>

#include <scroll_fixture.hpp>

#include <gtest/gtest.h>

#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace arisa::runtime {

using composition::Element;
using composition::NavigationPath;
using composition::NavigationSplitViewColumn;

class NavigationParityTest : public ScrollFixture {
 protected:
  class Host final : public composition::Component {
   public:
    using Build = std::function<Element(composition::Context&)>;

    explicit Host(std::shared_ptr<const Build> build)
        : build_(std::move(build)) {}

    auto operator==(const Host& other) const -> bool {
      return build_ == other.build_;
    }

    auto body(composition::Context& context) const -> Element override {
      return (*build_)(context);
    }

   private:
    std::shared_ptr<const Build> build_;
  };

  static auto host(Host::Build build) -> Element {
    return Host{std::make_shared<const Host::Build>(std::move(build))};
  }

  static auto pushed(const int& value) -> Element {
    return components::Text(std::format("detail {}", value))
        .frame({.max_width = composition::infinity,
                .max_height = composition::infinity})
        .id("pushed");
  }

  static auto count_nodes(const composition::Node& node) -> std::size_t {
    std::size_t total = 1;
    for (const composition::Node& child : node.children()) {
      total += count_nodes(child);
    }
    return total;
  }

  static auto compact(Element content) -> Element {
    return std::move(content).environment(
        components::Environment::horizontal_size_class,
        components::UserInterfaceSizeClass::compact);
  }
};

TEST_F(NavigationParityTest, PathRoundTripsThroughItsCodableRepresentation) {
  NavigationPath path{std::vector<int>{1, 2}};
  path.append(std::string{"three"});
  EXPECT_EQ(path.count(), 3U);
  const std::optional<NavigationPath::CodableRepresentation> codable =
      path.codable();
  ASSERT_TRUE(codable.has_value());
  const std::optional<NavigationPath::CodableRepresentation> decoded =
      NavigationPath::CodableRepresentation::decoded(codable->encoded());
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(*decoded, *codable);
  const NavigationPath restored{*decoded};
  ASSERT_EQ(restored.count(), 3U);
  ASSERT_NE(restored.entries()[0].get<int>(), nullptr);
  EXPECT_EQ(*restored.entries()[1].get<int>(), 2);
  ASSERT_NE(restored.entries()[2].get<std::string>(), nullptr);
  EXPECT_EQ(*restored.entries()[2].get<std::string>(), "three");
  EXPECT_EQ(restored.entries()[2].get<int>(), nullptr);
  EXPECT_FALSE(
      NavigationPath::CodableRepresentation::decoded("9:broken").has_value());
}

TEST_F(NavigationParityTest, RestoredPathDrivesAStack) {
  static const composition::StateKey<NavigationPath> stored{
      "parity-path",
      NavigationPath{*NavigationPath::CodableRepresentation::decoded(
          NavigationPath{std::vector<int>{4}}.codable()->encoded())}};
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers, host([](composition::Context& context) {
        return presentation::NavigationStack(
            composition::Binding<NavigationPath>{context.state(stored)}, [] {
              return Element{components::Text("root")}.navigation_destination(
                  std::type_identity<int>{}, pushed);
            });
      }));
  settle(providers, application);
  EXPECT_TRUE(shows(application, "detail 4"));
}

TEST_F(NavigationParityTest, CrossFadePushesWithoutSliding) {
  static const composition::StateKey<std::vector<int>> pages{"parity-fade", {}};
  static std::optional<composition::StateHandle<std::vector<int>>> handle;
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers, host([](composition::Context& context) {
              handle = context.state(pages);
              return presentation::NavigationStack(*handle, [] {
                return Element{components::Text("root")}.navigation_destination(
                    std::type_identity<int>{}, [](const int& value) {
                      return pushed(value).navigation_transition(
                          composition::NavigationTransition::cross_fade());
                    });
              });
            }));
  frames(application, 3);
  handle->set(std::vector<int>{1});
  for (int index = 0; index < 4; ++index) {
    step(providers, application);
  }
  ASSERT_TRUE(node_of(application, "pushed").has_value());
  EXPECT_NEAR(presented_of(application, "pushed").x, 0.0F, 1.0F);
  settle(providers, application);
  EXPECT_TRUE(shows(application, "detail 1"));
  EXPECT_TRUE(composition::NavigationTransition::cross_fade().fades());
  EXPECT_FALSE(composition::NavigationTransition::automatic().fades());
}

TEST_F(NavigationParityTest, DetailLinkPushesIntoTheDetailColumn) {
  const auto run = [](bool detail) {
    provider::headless::HeadlessProviderFactory providers;
    Application application =
        start(providers, host([detail](composition::Context&) {
                return Element{structure::NavigationSplitView(
                    presentation::NavigationStack([detail] {
                      return Element{
                          presentation::NavigationLink(std::string{"Open"},
                                                       [] { return pushed(3); })
                              .is_detail_link(detail)
                              .id("open")};
                    }),
                    components::Text("detail root"))};
              }));
    frames(application, 3);
    EXPECT_TRUE(application.root().activate(composition::ElementKey{"open"}));
    settle(providers, application);
    EXPECT_TRUE(shows(application, "detail 3"));
    const composition::Rect detail_column =
        presented_of(application, structure::NavigationSplitView::detail_id);
    return presented_of(application, "pushed").x >= detail_column.x - 1.0F;
  };
  EXPECT_TRUE(run(true));
  EXPECT_FALSE(run(false));
}

TEST_F(NavigationParityTest, PreferredCompactColumnChoosesTheCompactPane) {
  static const composition::StateKey<NavigationSplitViewColumn> column{
      "parity-column", NavigationSplitViewColumn::sidebar};
  static std::optional<composition::StateHandle<NavigationSplitViewColumn>>
      handle;
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers, host([](composition::Context& context) {
        handle = context.state(column);
        return compact(Element{structure::NavigationSplitView(
            composition::Binding<NavigationSplitViewColumn>{*handle},
            components::Text("Side pane"), components::Text("Detail pane"))});
      }));
  frames(application, 3);
  EXPECT_GT(
      frame_of(application, structure::NavigationSplitView::sidebar_id).width,
      0.0F);
  handle->set(NavigationSplitViewColumn::detail);
  settle(providers, application);
  EXPECT_NEAR(
      frame_of(application, structure::NavigationSplitView::sidebar_id).width,
      0.0F, 0.5F);
  EXPECT_EQ(structure::NavigationSplitView::compact_column(
                structure::NavigationSplitView::Visibility::detail_only, false),
            NavigationSplitViewColumn::detail);
  provider::headless::HeadlessProviderFactory three;
  static const composition::StateKey<NavigationSplitViewColumn> middle{
      "parity-middle", NavigationSplitViewColumn::content};
  static const composition::StateKey<structure::NavigationSplitView::Visibility>
      visibility{"parity-visibility",
                 structure::NavigationSplitView::Visibility::automatic};
  Application columns = start(
      three, host([](composition::Context& context) {
        return compact(Element{structure::NavigationSplitView(
            composition::Binding<structure::NavigationSplitView::Visibility>{
                context.state(visibility)},
            composition::Binding<NavigationSplitViewColumn>{
                context.state(middle)},
            components::Text("Side pane"), components::Text("Middle pane"),
            components::Text("Detail pane"))});
      }));
  frames(columns, 3);
  EXPECT_NEAR(
      frame_of(columns, structure::NavigationSplitView::sidebar_id).width, 0.0F,
      0.5F);
  EXPECT_GT(frame_of(columns, structure::NavigationSplitView::content_id).width,
            0.0F);
  EXPECT_EQ(structure::NavigationSplitView::compact_visibility(
                NavigationSplitViewColumn::content, true),
            structure::NavigationSplitView::Visibility::double_column);
}

TEST_F(NavigationParityTest, SeparatorPutsDetailOnePointAfterContent) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers, host([](composition::Context&) {
              return Element{structure::NavigationSplitView(
                  components::Text("Side"), components::Text("Middle"),
                  components::Text("Detail"))};
            }));
  frames(application, 3);
  const composition::Rect content =
      frame_of(application, structure::NavigationSplitView::content_id);
  const composition::Rect detail =
      frame_of(application, structure::NavigationSplitView::detail_id);
  ASSERT_TRUE(node_of(application, "arisa.split.separator").has_value());
  EXPECT_FLOAT_EQ(frame_of(application, "arisa.split.separator").width,
                  structure::NavigationSplitView::separator_width);
  EXPECT_NEAR(detail.x,
              content.x + content.width +
                  structure::NavigationSplitView::separator_width,
              0.5F);
}

TEST_F(NavigationParityTest, CustomSplitStyleWrapsTheConfigurationBody) {
  class Framed {
   public:
    auto make_body(const structure::NavigationSplitViewStyleConfiguration& made)
        const -> Element {
      return components::VStack(components::Text("Styled frame"), made.body());
    }
  };
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers, host([](composition::Context&) {
              return Element{
                  structure::NavigationSplitView(components::Text("Side"),
                                                 components::Text("Detail"))}
                  .navigation_split_view_style(Framed{});
            }));
  frames(application, 3);
  EXPECT_TRUE(shows(application, "Styled frame"));
  EXPECT_TRUE(shows(application, "Detail"));
}

TEST_F(NavigationParityTest, NavigationViewFollowsItsStyle) {
  const auto run = [](std::optional<structure::NavigationViewStyle> style,
                      bool narrow) {
    provider::headless::HeadlessProviderFactory providers;
    Element view = structure::NavigationView(components::Text("Primary"),
                                             components::Text("Secondary"));
    if (style.has_value()) {
      view = std::move(view).navigation_view_style(*style);
    }
    if (narrow) {
      view = compact(std::move(view));
    }
    Application application = start(providers, std::move(view));
    frames(application, 3);
    return std::pair{shows(application, "Primary"),
                     shows(application, "Secondary")};
  };
  EXPECT_EQ(run(std::nullopt, false), std::pair(true, true));
  EXPECT_EQ(run(
                structure::NavigationViewStyle{
                    structure::DefaultNavigationViewStyle{}},
                true),
            std::pair(true, false));
  EXPECT_EQ(run(structure::NavigationViewStyle::stack, false),
            std::pair(true, false));
  EXPECT_EQ(run(
                structure::NavigationViewStyle{
                    structure::DoubleColumnNavigationViewStyle{}},
                true),
            std::pair(true, true));
  EXPECT_EQ(
      structure::NavigationViewStyle{structure::StackNavigationViewStyle{}},
      structure::NavigationViewStyle::stack);
  EXPECT_EQ(structure::NavigationViewStyle::columns.kind(),
            structure::NavigationViewStyle::Kind::columns);
  EXPECT_EQ(structure::NavigationViewStyle::automatic.kind(),
            structure::NavigationViewStyle::Kind::automatic);
}

TEST_F(NavigationParityTest, LinkIndicatorVisibilityHidesTheChevron) {
  const auto nodes = [](bool hidden, bool declared) {
    provider::headless::HeadlessProviderFactory providers;
    Element link = presentation::NavigationLink(7, std::string{"Open"});
    if (hidden && declared) {
      link = std::move(link).navigation_link_indicator_visibility(
          composition::Visibility::hidden);
    }
    Element form = structure::Form(std::move(link));
    if (hidden && !declared) {
      form = std::move(form).navigation_link_indicator_visibility(
          composition::Visibility::hidden);
    }
    Application application = start(
        providers,
        presentation::NavigationStack(
            [held =
                 std::make_shared<composition::PlaceholderContentView::Source>(
                     std::move(form))] {
              return Element{composition::PlaceholderContentView(held)};
            }));
    frames(application, 3);
    return count_nodes(application.root().tree().root());
  };
  const std::size_t shown = nodes(false, false);
  EXPECT_LT(nodes(true, true), shown);
  EXPECT_LT(nodes(true, false), shown);
}

TEST_F(NavigationParityTest, AutomaticTitleModeFollowsThePreviousPage) {
  static const composition::StateKey<std::vector<int>> pages{
      "parity-title", std::vector<int>{1}};
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers, host([](composition::Context& context) {
        return presentation::NavigationStack(context.state(pages), [] {
          return Element{components::Text("root")}
              .navigation_title("Library")
              .navigation_bar_title_display_mode(
                  composition::ToolbarTitleDisplayMode::large)
              .navigation_destination(
                  std::type_identity<int>{}, [](const int& value) {
                    return pushed(value)
                        .navigation_title("Album")
                        .navigation_bar_title_display_mode(
                            composition::ToolbarTitleDisplayMode::automatic);
                  });
        });
      }));
  settle(providers, application);
  EXPECT_TRUE(shows(application, "detail 1"));
  EXPECT_TRUE(
      node_of(application, presentation::NavigationStack::large_title_id)
          .has_value());
}

TEST_F(NavigationParityTest, RightToLeftSwipeBackStartsAtTheTrailingEdge) {
  static const composition::StateKey<std::vector<int>> pages{
      "parity-rtl", std::vector<int>{1}};
  static std::optional<composition::StateHandle<std::vector<int>>> handle;
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers, host([](composition::Context& context) {
              handle = context.state(pages);
              return Element{presentation::NavigationStack(
                                 *handle,
                                 [] {
                                   return Element{components::Text("root")}
                                       .navigation_destination(
                                           std::type_identity<int>{}, pushed);
                                 })}
                  .environment(components::Environment::layout_direction,
                               composition::LayoutDirection::right_to_left);
            }));
  settle(providers, application);
  ASSERT_TRUE(shows(application, "detail 1"));
  drag(providers, application, {.x = 6.0F, .y = 360.0F},
       {.x = 700.0F, .y = 360.0F}, 10);
  settle(providers, application);
  EXPECT_EQ(handle->get().size(), 1U);
  drag(providers, application, {.x = 1274.0F, .y = 360.0F},
       {.x = 500.0F, .y = 360.0F}, 10);
  settle(providers, application);
  EXPECT_TRUE(handle->get().empty());
  EXPECT_TRUE(shows(application, "root"));
}

TEST_F(NavigationParityTest, TabBarVisibilityReachesTheTabViewFromAStackPage) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers,
      structure::TabView(
          structure::Tab(
              "One", "home", 0,
              [] {
                return presentation::NavigationStack([] {
                  return Element{components::Text("inside")}.toolbar_visibility(
                      composition::Visibility::hidden,
                      composition::ToolbarPlacement::tab_bar);
                });
              }),
          structure::Tab("Two", "star", 1,
                         [] { return components::Text("other"); })));
  settle(providers, application);
  EXPECT_TRUE(shows(application, "inside"));
  EXPECT_FALSE(
      node_of(application, structure::TabView::item_key(1).description())
          .has_value());
}

TEST_F(NavigationParityTest, ColourTransfersAsAColourRepresentation) {
  static_assert(composition::Transferable<theme::ColourValue>);
  const theme::ColourValue colour = theme::Colour::hex(0x3366CC);
  const std::optional<theme::ColourValue> decoded =
      theme::ColourValue::decoded(colour.encoded());
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(*decoded, colour);
  EXPECT_EQ(theme::ColourValue::content_type(), "public.color");
  EXPECT_FALSE(theme::ColourValue::decoded("not a colour").has_value());
}

}
