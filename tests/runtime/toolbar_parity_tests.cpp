#include <arisa/components/button.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
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
#include <arisa/composition/namespace.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/toolbar_customization.hpp>
#include <arisa/composition/toolbar_environment.hpp>
#include <arisa/composition/toolbar_item.hpp>
#include <arisa/composition/user_defaults.hpp>
#include <arisa/controls/searchable.hpp>
#include <arisa/presentation/navigation_stack.hpp>
#include <arisa/presentation/toolbar_layout.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/structure/scaffold.hpp>
#include <arisa/structure/tab.hpp>
#include <arisa/structure/tab_view.hpp>

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
using composition::infinity;
using composition::ToolbarItem;
using composition::ToolbarItemPlacement;
using presentation::ToolbarLayout;

class ToolbarParityTest : public ScrollFixture {
 protected:
  static auto item(std::string key, float width = 60.0F) -> Element {
    return components::Text(key)
        .frame({.width = width, .height = 30.0F})
        .id(std::move(key));
  }

  static auto rows(int count) -> Element {
    std::vector<Element> lines;
    for (int index = 0; index < count; ++index) {
      lines.push_back(
          components::Text(std::format("row {}", index))
              .frame({.height = 44.0F, .max_width = infinity})
              .id(composition::ElementKey{static_cast<std::int64_t>(index)}));
    }
    return components::ScrollView(components::VStack(std::move(lines)))
        .frame({.max_width = infinity, .max_height = infinity})
        .id("scroller");
  }

  static auto stacked(Element root) -> Element {
    auto held = std::make_shared<composition::PlaceholderContentView::Source>(
        std::move(root));
    return presentation::NavigationStack(
        [held] { return Element{composition::PlaceholderContentView(held)}; });
  }

  static auto visible(Application& application, std::string_view key) -> bool {
    const auto found = node_of(application, key);
    if (!found.has_value()) {
      return false;
    }
    const composition::Tree& tree = application.root().tree();
    for (std::optional<composition::NodeId> at = found; at.has_value();) {
      const auto node = tree.node(*at);
      if (!node.has_value()) {
        return false;
      }
      if (node->get().modifiers().opacity <= 0.0F) {
        return false;
      }
      at = node->get().parent();
    }
    return true;
  }

  static auto resize(provider::headless::HeadlessProviderFactory& providers,
                     unsigned width, unsigned height) {
    providers.surface().resize(
        provider::SurfaceSize{.width = provider::SurfaceWidth{width},
                              .height = provider::SurfaceHeight{height}});
  }
};

TEST_F(ToolbarParityTest, EveryPlacementLandsOnItsSide) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers,
      stacked(components::Text("body")
                  .navigation_title("Hidden title")
                  .toolbar(
                      ToolbarItem(ToolbarItemPlacement::navigation,
                                  item("navigation")),
                      ToolbarItem(ToolbarItemPlacement::destructive_action,
                                  item("destroy")),
                      ToolbarItem(ToolbarItemPlacement::top_bar_pinned_trailing,
                                  item("pinned")),
                      ToolbarItem(ToolbarItemPlacement::title,
                                  item("custom title", 120.0F)),
                      ToolbarItem(ToolbarItemPlacement::subtitle,
                                  item("custom subtitle", 120.0F)))));
  frames(application, 4);
  const composition::Rect bar =
      frame_of(application, presentation::NavigationStack::bar_id);
  const float middle = bar.x + (bar.width * 0.5F);
  EXPECT_LT(frame_of(application, "navigation").x, middle);
  EXPECT_GT(frame_of(application, "destroy").x, middle);
  EXPECT_GT(frame_of(application, "pinned").x,
            frame_of(application, "destroy").x);
  EXPECT_TRUE(shows(application, "custom title"));
  EXPECT_TRUE(shows(application, "custom subtitle"));
  EXPECT_FALSE(shows(application, "Hidden title"));
  EXPECT_LT(frame_of(application, "custom title").y,
            frame_of(application, "custom subtitle").y);
}

TEST_F(ToolbarParityTest, LargeTitlePlacementsReplaceTheLargeHeading) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers,
      stacked(rows(6)
                  .navigation_title("Plain")
                  .navigation_bar_title_display_mode(
                      composition::ToolbarTitleDisplayMode::large)
                  .toolbar(ToolbarItem(ToolbarItemPlacement::large_title,
                                       item("Big heading", 200.0F)),
                           ToolbarItem(ToolbarItemPlacement::large_subtitle,
                                       item("Big subtitle", 200.0F)))));
  frames(application, 4);
  ASSERT_TRUE(
      node_of(application, presentation::NavigationStack::large_title_id)
          .has_value());
  EXPECT_TRUE(shows(application, "Big heading"));
  EXPECT_TRUE(shows(application, "Big subtitle"));
  EXPECT_FALSE(shows(application, "Plain"));
}

TEST_F(ToolbarParityTest, ItemsThatDoNotFitMoveToTheOverflowByPriority) {
  provider::headless::HeadlessProviderFactory providers;
  resize(providers, 340, 600);
  const auto built = [](std::string key) {
    return [key] { return item(key, 90.0F); };
  };
  Application application = start(
      providers,
      stacked(components::Text("body").navigation_title("Narrow").toolbar(
          ToolbarItem(ToolbarItemPlacement::primary_action, built("low"))
              .visibility_priority(
                  composition::ToolbarItemVisibilityPriority::low),
          ToolbarItem(ToolbarItemPlacement::primary_action, built("normal")),
          ToolbarItem(ToolbarItemPlacement::primary_action, built("high"))
              .visibility_priority(
                  composition::ToolbarItemVisibilityPriority::high),
          ToolbarItem(ToolbarItemPlacement::primary_action, built("higher"))
              .visibility_priority(
                  composition::ToolbarItemVisibilityPriority::higher_than(
                      composition::ToolbarItemVisibilityPriority::high)))));
  frames(application, 8);
  EXPECT_FALSE(node_of(application, "low").has_value());
  EXPECT_TRUE(node_of(application, "high").has_value());
  EXPECT_TRUE(node_of(application, "higher").has_value());
  EXPECT_TRUE(node_of(application, ToolbarLayout::overflow_key).has_value());
  EXPECT_LT(composition::ToolbarItemVisibilityPriority::lower_than(
                composition::ToolbarItemVisibilityPriority::high),
            composition::ToolbarItemVisibilityPriority::high);
  EXPECT_GT(composition::ToolbarItemVisibilityPriority::lower_than(
                composition::ToolbarItemVisibilityPriority::high),
            composition::ToolbarItemVisibilityPriority::automatic);
  EXPECT_EQ(composition::ToolbarItemVisibilityPriority::higher_than(
                composition::ToolbarItemVisibilityPriority::high),
            composition::ToolbarItemVisibilityPriority::higher_than(
                composition::ToolbarItemVisibilityPriority::high));
}

TEST_F(ToolbarParityTest, CustomizableToolbarPersistsThroughAppStorage) {
  provider::headless::HeadlessProviderFactory providers;
  const composition::UserDefaults store;
  Application application = start(
      providers,
      stacked(components::Text("body").navigation_title("Custom").toolbar(
                  "main",
                  ToolbarItem("add", ToolbarItemPlacement::primary_action,
                              [] { return item("add"); }),
                  ToolbarItem("share", ToolbarItemPlacement::primary_action,
                              [] { return item("share"); })
                      .default_customization(
                          composition::Visibility::hidden,
                          composition::ToolbarCustomizationOptions::
                              always_available),
                  ToolbarItem("fixed", ToolbarItemPlacement::primary_action,
                              [] { return item("fixed"); })
                      .customization_behavior(
                          composition::ToolbarCustomizationBehavior::disabled)))
          .default_app_storage(store));
  frames(application, 4);
  EXPECT_TRUE(node_of(application, "add").has_value());
  EXPECT_FALSE(node_of(application, "share").has_value());
  EXPECT_TRUE(node_of(application, "fixed").has_value());
  tap(providers, application, centre(application, ToolbarLayout::overflow_key));
  frames(application, 4);
  EXPECT_TRUE(shows(application, "share"));
  ASSERT_TRUE(application.root().activate(
      composition::ElementKey{ToolbarLayout::customize_key}));
  frames(application, 6);
  ASSERT_TRUE(node_of(application, ToolbarLayout::editor_key).has_value());
  EXPECT_TRUE(
      node_of(application,
              std::string{ToolbarLayout::Editor::row_key("add").description()})
          .has_value());
  EXPECT_FALSE(
      node_of(
          application,
          std::string{ToolbarLayout::Editor::row_key("fixed").description()})
          .has_value());
  ASSERT_TRUE(
      application.root().activate(ToolbarLayout::Editor::row_key("add")));
  frames(application, 4);
  const std::optional<composition::ToolbarCustomization> saved =
      store.get<composition::ToolbarCustomization>(
          ToolbarLayout::storage_key("main"));
  ASSERT_TRUE(saved.has_value());
  EXPECT_EQ(saved->visibility("add"), composition::Visibility::hidden);
  ASSERT_TRUE(application.root().activate(
      composition::ElementKey{"arisa.toolbar.editor.done"}));
  frames(application, 30);
  EXPECT_FALSE(node_of(application, "add").has_value());
  const auto restored =
      composition::ToolbarCustomization::decoded(saved->encoded());
  ASSERT_TRUE(restored.has_value());
  EXPECT_EQ(*restored, *saved);
}

TEST_F(ToolbarParityTest, ItemModifiersShapeTheirGroups) {
  using Items = ToolbarLayout::Items;
  Items items;
  items.push_back(std::make_shared<const ToolbarItem>(
      ToolbarItem(ToolbarItemPlacement::primary_action, item("a"))));
  items.push_back(std::make_shared<const ToolbarItem>(
      ToolbarItem(ToolbarItemPlacement::primary_action, item("b"))
          .shared_background_visibility(composition::Visibility::hidden)));
  items.push_back(std::make_shared<const ToolbarItem>(
      ToolbarItem(ToolbarItemPlacement::primary_action, item("c"))
          .content_margins_removed()));
  items.push_back(std::make_shared<const ToolbarItem>(
      ToolbarItem(ToolbarItemPlacement::primary_action, item("d")).hidden()));
  items.push_back(std::make_shared<const ToolbarItem>(
      ToolbarItem(ToolbarItemPlacement::principal, item("e"))
          .axis_behavior(
              composition::ToolbarItemAxisBehavior::horizontal_only)));
  const ToolbarLayout layout{items, composition::ToolbarRole::automatic};
  const std::vector<ToolbarLayout::Segment> found =
      layout.segments(ToolbarLayout::Side::trailing);
  ASSERT_EQ(found.size(), 3U);
  EXPECT_FALSE(found[0].bare);
  EXPECT_TRUE(found[1].bare);
  EXPECT_TRUE(found[2].tight);
  EXPECT_TRUE(layout.has(ToolbarLayout::Side::principal));
  EXPECT_FALSE(layout.has(ToolbarLayout::Side::vertical));
}

TEST_F(ToolbarParityTest, ToolbarItemCarriesAZoomTransitionSource) {
  provider::headless::HeadlessProviderFactory providers;
  static constexpr composition::Namespace zoom{"toolbar-zoom"};
  class Zooming final : public composition::Component {
   public:
    auto operator==(const Zooming&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const composition::Namespace::ID space = context.namespace_id(zoom);
      return stacked(components::Text("body").toolbar(
          ToolbarItem(ToolbarItemPlacement::primary_action, item("zoomed"))
              .matched_transition_source(composition::ElementKey{"zoomed"},
                                         space)));
    }
  };
  Application application = start(providers, Zooming{});
  frames(application, 4);
  bool sourced = false;
  const auto search = [&](this const auto& self,
                          const composition::Node& node) -> void {
    sourced = sourced || node.matched_geometry().has_value();
    for (const composition::Node& child : node.children()) {
      self(child);
    }
  };
  search(application.root().tree().root());
  EXPECT_TRUE(sourced);
  EXPECT_TRUE(node_of(application, "zoomed").has_value());
}

TEST_F(ToolbarParityTest, DefaultToolbarItemMovesAndRemovesTheSearchField) {
  provider::headless::HeadlessProviderFactory providers;
  static constexpr composition::StateKey<std::string> query{"parity-query", ""};
  class Moving final : public composition::Component {
   public:
    explicit Moving(bool removes) noexcept : removes_(removes) {}

    auto operator==(const Moving&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const auto text = context.state(query);
      Element page =
          components::Text("results")
              .id("results")
              .searchable(text, composition::SearchFieldPlacement::toolbar)
              .toolbar(composition::DefaultToolbarItem(
                  composition::ToolbarDefaultItemKind::search,
                  ToolbarItemPlacement::bottom_bar));
      if (removes_) {
        page = std::move(page).toolbar(composition::ToolbarRemoval{
            .removing = composition::ToolbarDefaultItemKind::search});
      }
      return stacked(std::move(page).navigation_title("Find"));
    }

   private:
    bool removes_;
  };
  Application application = start(providers, Moving{false});
  frames(application, 4);
  ASSERT_TRUE(node_of(application, "arisa.search.bar").has_value());
  EXPECT_GT(frame_of(application, "arisa.search.bar").y,
            frame_of(application, "results").y);
  provider::headless::HeadlessProviderFactory removing;
  Application removed = start(removing, Moving{true});
  frames(removed, 4);
  EXPECT_FALSE(node_of(removed, "arisa.search.bar").has_value());
}

TEST_F(ToolbarParityTest, TitleAlignmentAndEdgesReachTheItems) {
  static constexpr composition::StateKey<int> unused{"parity-unused", 0};
  struct Seen {
    std::optional<composition::ToolbarTitleAlignment> alignment;
    std::vector<std::optional<composition::VerticalEdge>> edges;
  };
  static Seen seen;
  class Probe final : public composition::Component {
   public:
    explicit Probe(std::string key) : key_(std::move(key)) {}

    auto operator==(const Probe&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      static_cast<void>(context.state(unused));
      seen.alignment =
          context.environment(composition::ToolbarEnvironment::title_alignment);
      seen.edges.push_back(
          context.environment(composition::ToolbarEnvironment::vertical_edge));
      return item(key_);
    }

   private:
    std::string key_;
  };
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers,
      stacked(components::Text("body")
                  .navigation_title("Centred")
                  .toolbar_title_alignment(
                      composition::ToolbarTitleAlignment::center)
                  .toolbar(ToolbarItem(ToolbarItemPlacement::primary_action,
                                       Element{Probe{"top"}}),
                           ToolbarItem(ToolbarItemPlacement::bottom_bar,
                                       Element{Probe{"bottom"}}))));
  frames(application, 4);
  const composition::Rect bar =
      frame_of(application, presentation::NavigationStack::bar_id);
  bool centred = false;
  const auto title = [&]() -> std::optional<composition::Rect> {
    for (const std::string& text : texts(application)) {
      if (text == "Centred") {
        centred = true;
      }
    }
    return std::nullopt;
  }();
  static_cast<void>(title);
  EXPECT_TRUE(centred);
  EXPECT_EQ(seen.alignment, composition::ToolbarTitleAlignment::center);
  EXPECT_TRUE(std::ranges::contains(seen.edges,
                                    std::optional<composition::VerticalEdge>{
                                        composition::VerticalEdge::top}));
  EXPECT_TRUE(std::ranges::contains(seen.edges,
                                    std::optional<composition::VerticalEdge>{
                                        composition::VerticalEdge::bottom}));
  EXPECT_GT(bar.width, 0.0F);
}

TEST_F(ToolbarParityTest, NavigationBarMinimizesOnScrollAndRestores) {
  const auto page =
      [](composition::ToolbarMinimizationRestoration restoration,
         composition::ToolbarMinimizationSafeAreaAdjustment adjustment) {
        return stacked(
            rows(60)
                .navigation_title("Feed")
                .toolbar(ToolbarItem(ToolbarItemPlacement::primary_action,
                                     item("action")))
                .toolbar_minimization_behavior(
                    composition::ToolbarMinimizationBehavior::on_scroll_down,
                    composition::ToolbarPlacement::navigation_bar)
                .toolbar_minimization_restoration(
                    restoration, composition::ToolbarPlacement::navigation_bar)
                .toolbar_minimization_safe_area_adjustment(
                    adjustment, composition::ToolbarPlacement::navigation_bar));
      };
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers,
      page(composition::ToolbarMinimizationRestoration::automatic,
           composition::ToolbarMinimizationSafeAreaAdjustment::automatic));
  frames(application, 4);
  const float full =
      frame_of(application, presentation::NavigationStack::bar_id).height;
  EXPECT_TRUE(visible(application, "action"));
  drag(providers, application, {.x = 600.0F, .y = 600.0F},
       {.x = 600.0F, .y = 200.0F}, 8);
  settle(providers, application);
  EXPECT_LT(frame_of(application, presentation::NavigationStack::bar_id).height,
            full);
  EXPECT_FALSE(visible(application, "action"));
  drag(providers, application, {.x = 600.0F, .y = 300.0F},
       {.x = 600.0F, .y = 400.0F}, 6);
  settle(providers, application);
  EXPECT_TRUE(visible(application, "action"));

  provider::headless::HeadlessProviderFactory edge_providers;
  Application edge =
      start(edge_providers,
            page(composition::ToolbarMinimizationRestoration::at_scroll_edge,
                 composition::ToolbarMinimizationSafeAreaAdjustment::disabled));
  frames(edge, 4);
  drag(edge_providers, edge, {.x = 600.0F, .y = 600.0F},
       {.x = 600.0F, .y = 150.0F}, 8);
  settle(edge_providers, edge);
  EXPECT_FALSE(visible(edge, "action"));
  EXPECT_NEAR(frame_of(edge, presentation::NavigationStack::bar_id).height,
              full, 1.0F);
  drag(edge_providers, edge, {.x = 600.0F, .y = 300.0F},
       {.x = 600.0F, .y = 360.0F}, 4);
  settle(edge_providers, edge);
  EXPECT_FALSE(visible(edge, "action"));
}

TEST_F(ToolbarParityTest, VerticalPreferredItemsUseTheVerticalBar) {
  const auto page = [](bool disabled) {
    Element content =
        components::Text("body")
            .navigation_title("Vertical")
            .toolbar(
                ToolbarItem(ToolbarItemPlacement::primary_action,
                            item("upright"))
                    .axis_behavior(composition::ToolbarItemAxisBehavior::
                                       vertical_preferred),
                ToolbarItem(ToolbarItemPlacement::primary_action, item("level"))
                    .axis_behavior(
                        composition::ToolbarItemAxisBehavior::horizontal_only));
    if (disabled) {
      content = std::move(content).toolbar_vertical_behavior(
          composition::ToolbarVerticalBehavior::disabled);
    }
    return stacked(std::move(content));
  };
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, page(false));
  frames(application, 4);
  ASSERT_TRUE(node_of(application, ToolbarLayout::vertical_key).has_value());
  const composition::Rect upright = frame_of(application, "upright");
  const composition::Rect level = frame_of(application, "level");
  EXPECT_GT(upright.y, level.y + level.height);
  provider::headless::HeadlessProviderFactory flat_providers;
  Application flat = start(flat_providers, page(true));
  frames(flat, 4);
  EXPECT_FALSE(node_of(flat, ToolbarLayout::vertical_key).has_value());
  EXPECT_NEAR(frame_of(flat, "upright").y, frame_of(flat, "level").y, 1.0F);
}

TEST_F(ToolbarParityTest, CompactHeightCompressesTheChosenBars) {
  const auto tabs =
      [](composition::ToolbarVerticalCompressionBehavior squeeze) {
        return Element{
            structure::TabView(
                structure::Tab(
                    "One", "home", 0,
                    [squeeze] {
                      return stacked(
                          components::Text("first")
                              .navigation_title("Compressed")
                              .toolbar(ToolbarItem(
                                  ToolbarItemPlacement::primary_action,
                                  item("action")))
                              .toolbar_vertical_compression_behavior(squeeze));
                    }),
                structure::Tab("Two", "star", 1,
                               [] { return components::Text("second"); }))}
            .environment(components::Environment::vertical_size_class,
                         components::UserInterfaceSizeClass::compact);
      };
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers,
      tabs(composition::ToolbarVerticalCompressionBehavior::prefers_tab_bar));
  frames(application, 6);
  EXPECT_FALSE(node_of(application, "action").has_value());
  EXPECT_TRUE(
      node_of(application,
              std::string{structure::TabView::item_key(1).description()})
          .has_value());
  provider::headless::HeadlessProviderFactory other;
  Application items =
      start(other, tabs(composition::ToolbarVerticalCompressionBehavior::
                            prefers_toolbar_items));
  frames(items, 6);
  EXPECT_TRUE(node_of(items, "action").has_value());
  EXPECT_FALSE(
      node_of(items, std::string{structure::TabView::item_key(1).description()})
          .has_value());
}

TEST_F(ToolbarParityTest, OverflowMenuCollectsDeclaredContent) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers, stacked(components::Text("body")
                             .navigation_title("Overflow")
                             .toolbar_overflow_menu([] {
                               return components::Button("Archive all", [] {});
                             })
                             .toolbar(composition::ToolbarOverflowMenu([] {
                               return components::Button("Export all", [] {});
                             }))));
  frames(application, 4);
  ASSERT_TRUE(node_of(application, ToolbarLayout::overflow_key).has_value());
  EXPECT_FALSE(shows(application, "Archive all"));
  tap(providers, application, centre(application, ToolbarLayout::overflow_key));
  frames(application, 4);
  EXPECT_TRUE(shows(application, "Archive all"));
  EXPECT_TRUE(shows(application, "Export all"));
}

TEST_F(ToolbarParityTest, LabelledGroupCollapsesIntoItsLabel) {
  provider::headless::HeadlessProviderFactory providers;
  resize(providers, 360, 600);
  std::vector<Element> grouped;
  grouped.push_back(item("bold", 110.0F));
  grouped.push_back(item("italic", 110.0F));
  grouped.push_back(item("underline", 110.0F));
  Application application =
      start(providers,
            stacked(components::Text("body").navigation_title("Format").toolbar(
                composition::ToolbarItemGroup(
                    ToolbarItemPlacement::primary_action, std::move(grouped),
                    components::Text("Format").id("group-label")))));
  frames(application, 8);
  EXPECT_TRUE(shows(application, "Format"));
  EXPECT_FALSE(node_of(application, "underline").has_value() &&
               node_of(application, "bold").has_value() &&
               frame_of(application, "underline").x >
                   frame_of(application, "bold").x + 300.0F);
}

TEST_F(ToolbarParityTest, StatusBarVisibilityHidesTheScaffoldStatus) {
  provider::headless::HeadlessProviderFactory providers;
  Application shown = start(
      providers,
      structure::Scaffold(components::Text("page").navigation_title("Shown")));
  frames(shown, 3);
  EXPECT_TRUE(node_of(shown, "arisa.scaffold.status").has_value());
  provider::headless::HeadlessProviderFactory hidden_providers;
  Application hidden = start(
      hidden_providers,
      structure::Scaffold(
          components::Text("page")
              .navigation_title("Hidden")
              .toolbar_visibility(composition::Visibility::hidden,
                                  composition::ToolbarPlacement::status_bar)));
  frames(hidden, 3);
  EXPECT_FALSE(node_of(hidden, "arisa.scaffold.status").has_value());
}

TEST_F(ToolbarParityTest, ToolbarItemHiddenShowsOnlyWhileCustomizing) {
  const auto group = [](bool customizing) {
    return Element{
        components::HStack(
            components::Text("always").id("always"),
            components::Text("collab").id("collab").toolbar_item_hidden(),
            components::Text("visible").id("visible").toolbar_item_hidden(
                false))}
        .environment(composition::ToolbarEnvironment::customizing, customizing);
  };
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, group(false));
  frames(application, 3);
  EXPECT_FALSE(shows(application, "collab"));
  EXPECT_TRUE(shows(application, "visible"));
  provider::headless::HeadlessProviderFactory editing;
  Application customizing = start(editing, group(true));
  frames(customizing, 3);
  EXPECT_TRUE(shows(customizing, "collab"));
}

TEST_F(ToolbarParityTest, OnScrollUpMinimizesWhenScrollingBack) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers,
      stacked(
          rows(60)
              .navigation_title("Feed")
              .toolbar(ToolbarItem(ToolbarItemPlacement::primary_action,
                                   item("action")))
              .toolbar_minimization_behavior(
                  composition::ToolbarMinimizationBehavior::on_scroll_up,
                  composition::ToolbarPlacement::navigation_bar)
              .toolbar_minimization_safe_area_adjustment(
                  composition::ToolbarMinimizationSafeAreaAdjustment::enabled,
                  composition::ToolbarPlacement::navigation_bar)));
  frames(application, 4);
  const float full =
      frame_of(application, presentation::NavigationStack::bar_id).height;
  drag(providers, application, {.x = 600.0F, .y = 600.0F},
       {.x = 600.0F, .y = 200.0F}, 8);
  settle(providers, application);
  EXPECT_TRUE(visible(application, "action"));
  drag(providers, application, {.x = 600.0F, .y = 300.0F},
       {.x = 600.0F, .y = 400.0F}, 6);
  settle(providers, application);
  EXPECT_FALSE(visible(application, "action"));
  EXPECT_LT(frame_of(application, presentation::NavigationStack::bar_id).height,
            full);
}

TEST_F(ToolbarParityTest, AutomaticMinimizationFollowsAPrincipalSearchField) {
  static constexpr composition::StateKey<std::string> text{"parity-minimize",
                                                           ""};
  class Page final : public composition::Component {
   public:
    Page(bool searches, composition::ToolbarMinimizationBehavior behavior)
        : searches_(searches), behavior_(behavior) {}

    auto operator==(const Page&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      Element page =
          rows(60)
              .navigation_title("Feed")
              .toolbar(ToolbarItem(ToolbarItemPlacement::primary_action,
                                   item("action")))
              .toolbar_minimization_behavior(
                  behavior_, composition::ToolbarPlacement::navigation_bar);
      if (searches_) {
        page = std::move(page).searchable(
            context.state(text),
            composition::SearchFieldPlacement::toolbar_principal);
      }
      return stacked(std::move(page));
    }

   private:
    bool searches_;
    composition::ToolbarMinimizationBehavior behavior_;
  };
  const auto minimizes = [](bool searches,
                            composition::ToolbarMinimizationBehavior behavior) {
    provider::headless::HeadlessProviderFactory providers;
    Application application = start(providers, Page{searches, behavior});
    frames(application, 4);
    drag(providers, application, {.x = 600.0F, .y = 600.0F},
         {.x = 600.0F, .y = 200.0F}, 8);
    settle(providers, application);
    return !visible(application, "action");
  };
  EXPECT_TRUE(
      minimizes(true, composition::ToolbarMinimizationBehavior::automatic));
  EXPECT_FALSE(
      minimizes(false, composition::ToolbarMinimizationBehavior::automatic));
  EXPECT_FALSE(
      minimizes(true, composition::ToolbarMinimizationBehavior::never));
}

TEST_F(ToolbarParityTest, LeadingTitleAlignmentPinsTheTitleLeft) {
  const auto title_x = [](composition::ToolbarTitleAlignment alignment) {
    provider::headless::HeadlessProviderFactory providers;
    Application application = start(
        providers,
        stacked(components::Text("body")
                    .navigation_title("Pinned title")
                    .toolbar_role(composition::ToolbarRole::navigation_stack)
                    .toolbar_title_alignment(alignment)));
    frames(application, 4);
    float found = -1.0F;
    const auto search = [&](this const auto& self,
                            const composition::Node& node) -> void {
      if (const auto written = node.content_as<components::Text::Content>();
          written.has_value() && written->get().text() == "Pinned title" &&
          node.presented().has_value()) {
        found = node.presented()->bounds.x;
      }
      for (const composition::Node& child : node.children()) {
        self(child);
      }
    };
    search(application.root().tree().root());
    return found;
  };
  const float leading = title_x(composition::ToolbarTitleAlignment::leading);
  const float centred = title_x(composition::ToolbarTitleAlignment::automatic);
  EXPECT_GE(leading, 0.0F);
  EXPECT_LT(leading, 200.0F);
  EXPECT_GT(centred, 400.0F);
}

class ReadyPage final : public composition::Component {
 public:
  auto operator==(const ReadyPage&) const -> bool = default;

  auto body(composition::Context&) const -> Element override {
    return components::Text("body").navigation_title("Ready").toolbar(
        ToolbarItem(ToolbarItemPlacement::secondary_action,
                    Element{components::Button("Ready item", [] {})}));
  }
};

TEST_F(ToolbarParityTest, ReadyOverflowItemsShowEveryTimeThePopoverOpens) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, stacked(ReadyPage{}));
  frames(application, 4);
  ASSERT_TRUE(node_of(application, ToolbarLayout::overflow_key).has_value());
  for (int round = 0; round < 2; ++round) {
    tap(providers, application,
        centre(application, ToolbarLayout::overflow_key));
    frames(application, 20);
    EXPECT_TRUE(shows(application, "Ready item")) << round;
    press(providers, application, provider::Button::b);
    frames(application, 30);
    EXPECT_FALSE(shows(application, "Ready item")) << round;
  }
}

TEST_F(ToolbarParityTest, ReadyOverflowItemsOnAWindowRootStackShowEveryTime) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers, presentation::NavigationStack([] {
              return components::Text("body").navigation_title("Root").toolbar(
                  ToolbarItem(ToolbarItemPlacement::secondary_action,
                              Element{components::Button("Root item", [] {})}));
            }));
  frames(application, 4);
  ASSERT_TRUE(node_of(application, ToolbarLayout::overflow_key).has_value());
  for (int round = 0; round < 2; ++round) {
    tap(providers, application,
        centre(application, ToolbarLayout::overflow_key));
    frames(application, 20);
    EXPECT_TRUE(shows(application, "Root item")) << round;
    press(providers, application, provider::Button::b);
    frames(application, 30);
    EXPECT_FALSE(shows(application, "Root item")) << round;
  }
}

}
