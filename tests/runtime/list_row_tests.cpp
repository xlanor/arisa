#include <arisa/components/button.hpp>
#include <arisa/components/button_role.hpp>
#include <arisa/components/edit_actions.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/for_each.hpp>
#include <arisa/components/grid_item.hpp>
#include <arisa/components/grid_section.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/lazy_vgrid.hpp>
#include <arisa/components/lazy_vstack.hpp>
#include <arisa/components/pinned_scrollable_views.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/horizontal_edge.hpp>
#include <arisa/composition/hover_effect_environment.hpp>
#include <arisa/composition/list_row.hpp>
#include <arisa/composition/prominence.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/vertical_edges.hpp>
#include <arisa/composition/visibility.hpp>
#include <arisa/controls/context_menu.hpp>
#include <arisa/motion/hover_effect.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/structure/cell_style.hpp>
#include <arisa/structure/disclosure_group.hpp>
#include <arisa/structure/list.hpp>
#include <arisa/structure/list_cell.hpp>
#include <arisa/structure/list_style.hpp>
#include <arisa/structure/outline_group.hpp>
#include <arisa/structure/section.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/shape.hpp>

#include <scroll_fixture.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <format>
#include <memory>
#include <optional>
#include <ranges>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace arisa::runtime {

using composition::Element;
using composition::infinity;
using provider::Button;

class ListRowTest : public ScrollFixture {
 protected:
  static auto node_with_text(const composition::Node& node,
                             std::string_view text)
      -> const composition::Node* {
    if (const auto shown = node.content_as<components::Text::Content>();
        shown.has_value() && shown->get().text() == text) {
      return &node;
    }
    for (const composition::Node& child : node.children()) {
      if (const composition::Node* found = node_with_text(child, text)) {
        return found;
      }
    }
    return nullptr;
  }

  struct Seen {
    bool first{false};
    bool last{false};
    bool separator{true};
    std::optional<theme::ColourValue> tint;
    std::optional<theme::Insets> insets;
    bool background{false};
    std::optional<float> minimum;
  };

  class Recorder {
   public:
    explicit Recorder(std::shared_ptr<std::vector<Seen>> log) noexcept
        : log_(std::move(log)) {}

    auto make_body(structure::CellStyle::Configuration configuration) const
        -> Element {
      log_->push_back(Seen{.first = configuration.is_first,
                           .last = configuration.is_last,
                           .separator = configuration.shows_separator,
                           .tint = configuration.separator_tint,
                           .insets = configuration.insets,
                           .background = configuration.background.has_value(),
                           .minimum = configuration.minimum_height});
      return std::move(configuration.content);
    }

    friend auto operator==(const Recorder& left, const Recorder& right)
        -> bool {
      return left.log_ == right.log_;
    }

   private:
    std::shared_ptr<std::vector<Seen>> log_;
  };

  static auto recorded(const std::shared_ptr<std::vector<Seen>>& log,
                       std::size_t count) -> std::vector<Seen> {
    if (log->size() < count) {
      return {};
    }
    return std::vector<Seen>(log->end() - static_cast<std::ptrdiff_t>(count),
                             log->end());
  }

  static void run(provider::headless::HeadlessProviderFactory& providers,
                  Application& application, int count) {
    for (int index = 0; index < count; ++index) {
      ScrollFixture::step(providers, application);
    }
  }

  static auto hinted(Application& application, Button button,
                     std::string_view label) -> bool {
    return std::ranges::any_of(
        application.interaction().hints(),
        [button, label](const composition::CommandHint& hint) {
          return hint.button == button && hint.label == label;
        });
  }

  static void hold_button(
      provider::headless::HeadlessProviderFactory& providers,
      Application& application, Button which,
      std::chrono::milliseconds duration) {
    hold(providers, application, pad({which}), duration);
  }

  class Names final : public composition::Component {
   public:
    static const composition::StateKey<std::vector<std::string>> names;

    explicit Names(std::shared_ptr<int> opened) noexcept
        : opened_(std::move(opened)) {}

    auto operator==(const Names& other) const -> bool {
      return opened_ == other.opened_;
    }

    auto body(composition::Context& context) const -> Element override {
      const auto all = context.state(names);
      const auto opened = opened_;
      return structure::List(structure::Section(components::ForEach(
          all.get(), [](const std::string& name) { return name; },
          [all, opened](const std::string& name) {
            const auto remove = [all, name] {
              all.update([&name](std::vector<std::string>& every) {
                std::erase(every, name);
              });
            };
            Element row = name == "Open" ? Element{components::Button(
                                               "Open", [opened] { ++*opened; })}
                                         : Element{components::Text(name)};
            return std::move(row)
                .badge(name == "Alpha" ? 5 : 0)
                .swipe_actions([remove] {
                  return Element{components::Button(
                      "Delete", components::ButtonRole::destructive, remove)};
                })
                .context_menu([remove] {
                  return components::Group(
                      components::Button("Pin", [] {}),
                      components::Button("Erase",
                                         components::ButtonRole::destructive,
                                         remove));
                })
                .id(name);
          })));
    }

   private:
    std::shared_ptr<int> opened_;
  };

  class Picks final : public composition::Component {
   public:
    static constexpr composition::StateKey<bool> editing{"picks-editing", true};
    static const composition::StateKey<std::vector<int>> rows;
    static const composition::StateKey<std::set<int>> chosen;

    explicit Picks(
        std::shared_ptr<std::optional<composition::StateHandle<std::set<int>>>>
            handle,
        std::shared_ptr<
            std::optional<composition::StateHandle<std::vector<int>>>>
            all) noexcept
        : handle_(std::move(handle)), all_(std::move(all)) {}

    auto operator==(const Picks& other) const -> bool {
      return handle_ == other.handle_;
    }

    auto body(composition::Context& context) const -> Element override {
      const auto picked = context.state(chosen);
      const auto every = context.state(rows);
      *handle_ = picked;
      *all_ = every;
      return structure::List(
                 picked,
                 structure::Section(components::ForEach(
                     every.get(), [](int row) { return row; },
                     [](int row) {
                       return components::Text(std::format("Row {}", row))
                           .tag(row)
                           .selection_disabled(row == 2);
                     })))
          .environment(components::Environment::edit_mode,
                       context.state(editing).get()
                           ? components::EditMode::active
                           : components::EditMode::inactive);
    }

   private:
    std::shared_ptr<std::optional<composition::StateHandle<std::set<int>>>>
        handle_;
    std::shared_ptr<std::optional<composition::StateHandle<std::vector<int>>>>
        all_;
  };

  class Tiles final : public composition::Component {
   public:
    static const composition::StateKey<std::vector<int>> order;

    explicit Tiles(std::shared_ptr<std::vector<int>> seen) noexcept
        : seen_(std::move(seen)) {}

    auto operator==(const Tiles& other) const -> bool {
      return seen_ == other.seen_;
    }

    auto body(composition::Context& context) const -> Element override {
      const auto tiles = context.state(order);
      *seen_ = tiles.get();
      return components::ScrollView(components::LazyVGrid(
          components::LazyVGridOptions{
              .columns = components::GridItem::repeated(
                  3, components::GridItem{components::GridItem::Size::fixed(
                         100.0F)})},
          components::ForEach(
              composition::Binding<std::vector<int>>{tiles},
              components::EditActions::move,
              [](int tile) { return std::format("tile-{}", tile); },
              [](int tile) {
                return components::Text(std::format("{}", tile))
                    .frame({.width = 100.0F, .height = 60.0F})
                    .focusable();
              })));
    }

   private:
    std::shared_ptr<std::vector<int>> seen_;
  };

  class Editable final : public composition::Component {
   public:
    static const composition::StateKey<std::vector<int>> items;

    auto operator==(const Editable&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const auto all = context.state(items);
      return structure::List(structure::Section(components::ForEach(
                                 composition::Binding<std::vector<int>>{all},
                                 components::EditActions::all,
                                 [](int item) { return item; },
                                 [](int item) {
                                   return components::Text(
                                              std::format("Item {}", item))
                                       .delete_disabled(item == 1)
                                       .move_disabled(item == 1);
                                 })))
          .environment(components::Environment::edit_mode,
                       components::EditMode::active);
    }
  };
  static auto pinned_list() -> Element {
    std::vector<components::GridSection> sections;
    for (int section = 0; section < 3; ++section) {
      sections.emplace_back(
          Element{components::Text(std::format("Header {}", section))
                      .frame({.height = 30.0F, .max_width = infinity})
                      .background(theme::ColourRole::destructive,
                                  theme::Shape::rectangle())
                      .id(std::format("header-{}", section))},
          components::ForEach(
              std::views::iota(0, 20),
              [section](int row) { return (section * 100) + row; },
              [](int row) {
                return components::Text(std::format("Item {}", row))
                    .frame({.height = 40.0F, .max_width = infinity});
              }));
    }
    return components::ScrollView(
               components::LazyVStack(
                   components::LazyVStackOptions{
                       .spacing = 0.0F,
                       .pinned_views =
                           components::PinnedScrollableViews::section_headers},
                   std::move(sections)))
        .frame({.width = 400.0F, .height = 300.0F})
        .id("pinned-scroll");
  }
};

const composition::StateKey<std::vector<std::string>> ListRowTest::Names::names{
    "list-row-names",
    std::vector<std::string>{"Alpha", "Bravo", "Open", "Delta"}};
const composition::StateKey<std::vector<int>> ListRowTest::Picks::rows{
    "picks-rows",
    std::views::iota(0, 1000) | std::ranges::to<std::vector<int>>()};
const composition::StateKey<std::set<int>> ListRowTest::Picks::chosen{
    "picks-chosen", std::set<int>{}};
const composition::StateKey<std::vector<int>> ListRowTest::Tiles::order{
    "tiles-order",
    std::views::iota(1, 10) | std::ranges::to<std::vector<int>>()};

TEST_F(ListRowTest, SeparatorsFollowBothNeighboursAsSwiftUIDoes) {
  auto log = std::make_shared<std::vector<Seen>>();
  provider::headless::HeadlessProviderFactory providers;
  const theme::Colour red = theme::Colour::hex(0xFF0000);
  Application application =
      start(providers,
            structure::List(
                structure::Section(
                    components::Text("Alpha"),
                    components::Text("Bravo")
                        .list_row_separator(composition::Visibility::hidden,
                                            composition::VerticalEdges::bottom)
                        .list_row_separator_tint(
                            red, composition::VerticalEdges::top),
                    components::Text("Charlie"), components::Text("Delta")))
                .cell_style(Recorder{log}));
  const std::vector<Seen> rows = recorded(log, 4);
  ASSERT_EQ(rows.size(), 4U);
  EXPECT_TRUE(rows[0].first);
  EXPECT_TRUE(rows[1].separator);
  ASSERT_TRUE(rows[1].tint.has_value());
  EXPECT_EQ(*rows[1].tint, theme::ColourValue{red});
  EXPECT_FALSE(rows[2].separator);
  EXPECT_FALSE(rows[2].tint.has_value());
  EXPECT_TRUE(rows[3].separator);
  EXPECT_TRUE(rows[3].last);
}

TEST_F(ListRowTest, RowLayoutModifiersReachTheCellStyle) {
  auto log = std::make_shared<std::vector<Seen>>();
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers,
            structure::List(structure::Section(
                                components::Text("Alpha").list_row_insets(
                                    theme::Insets::all(12.0F)),
                                components::Text("Bravo").list_row_background(
                                    components::Text("Back")),
                                components::Text("Charlie")))
                .default_min_list_row_height(60.0F)
                .list_row_spacing(6.0F)
                .cell_style(Recorder{log}));
  const std::vector<Seen> rows = recorded(log, 3);
  ASSERT_EQ(rows.size(), 3U);
  ASSERT_TRUE(rows[0].insets.has_value());
  EXPECT_EQ(*rows[0].insets, theme::Insets::all(12.0F));
  EXPECT_TRUE(rows[1].background);
  EXPECT_FALSE(rows[2].background);
  for (const Seen& row : rows) {
    EXPECT_EQ(row.minimum, std::optional<float>{60.0F});
    EXPECT_TRUE(row.first);
    EXPECT_TRUE(row.last);
    EXPECT_FALSE(row.separator);
  }
}

TEST_F(ListRowTest, AlternatingRowBackgroundsFillEveryOtherRow) {
  auto log = std::make_shared<std::vector<Seen>>();
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers,
            structure::List(structure::Section(components::Text("Alpha"),
                                               components::Text("Bravo"),
                                               components::Text("Charlie")))
                .alternating_row_backgrounds()
                .cell_style(Recorder{log}));
  ASSERT_GE(log->size(), 3U);
  const std::vector<Seen> rows(log->begin(), log->begin() + 3);
  EXPECT_FALSE(rows[0].background);
  EXPECT_TRUE(rows[1].background);
  EXPECT_FALSE(rows[2].background);
}

TEST_F(ListRowTest, ListSectionSpacingSeparatesSections) {
  const auto gap = [](std::optional<float> spacing) {
    provider::headless::HeadlessProviderFactory providers;
    Application application = start(
        providers,
        structure::List(structure::Section(
                            components::VStack()
                                .frame({.height = 20.0F, .max_width = infinity})
                                .id("a")),
                        structure::Section(
                            components::VStack()
                                .frame({.height = 20.0F, .max_width = infinity})
                                .id("b")))
            .list_style(structure::ListStyle::inset_grouped())
            .list_section_spacing(spacing));
    run(providers, application, 4);
    return presented_of(application, "b").y -
           (presented_of(application, "a").y +
            presented_of(application, "a").height);
  };
  const float standard = gap(std::nullopt);
  const float wide = gap(60.0F);
  EXPECT_GT(wide, standard + 20.0F);
  EXPECT_NEAR(gap(0.0F) + 60.0F, wide, 1.0F);
}

TEST_F(ListRowTest, IncreasedHeaderProminenceEnlargesTheHeader) {
  const auto font = [](composition::Prominence prominence) {
    provider::headless::HeadlessProviderFactory providers;
    Application application = start(
        providers,
        structure::List(structure::Section("Head", components::Text("Alpha")))
            .list_style(structure::ListStyle::plain())
            .header_prominence(prominence));
    run(providers, application, 4);
    const composition::Tree& tree = application.root().tree();
    const composition::Node* found = node_with_text(tree.root(), "Head");
    std::optional<theme::Font> chosen;
    while (found != nullptr && !chosen.has_value()) {
      if (const auto declared =
              found->override_of(components::Environment::font)) {
        chosen = declared->get();
      }
      const auto parent = found->parent();
      found = parent.has_value() ? &tree.node(*parent)->get() : nullptr;
    }
    return chosen;
  };
  EXPECT_EQ(font(composition::Prominence::standard),
            std::optional{theme::Font::subheadline});
  EXPECT_EQ(font(composition::Prominence::increased),
            std::optional{theme::Font::title3});
}

TEST_F(ListRowTest, ListRowHoverEffectReachesTheRow) {
  const auto hovering = [](const auto& self,
                           const composition::Node& node) -> bool {
    if (node.modifiers().hover.has_value()) {
      return true;
    }
    return std::ranges::any_of(
        node.children(),
        [&self](const composition::Node& child) { return self(self, child); });
  };
  provider::headless::HeadlessProviderFactory providers;
  Application plain =
      start(providers,
            structure::List(structure::Section(components::Text("Alpha"))));
  EXPECT_FALSE(hovering(hovering, plain.root().tree().root()));
  provider::headless::HeadlessProviderFactory lifted_providers;
  Application lifted = start(
      lifted_providers, structure::List(structure::Section(
                            components::Text("Alpha").list_row_hover_effect(
                                motion::HoverEffect::lift))));
  EXPECT_TRUE(hovering(hovering, lifted.root().tree().root()));
}

TEST_F(ListRowTest, RowHoverEffectCanBeDisabled) {
  const auto disabled = [](const auto& self,
                           const composition::Node& node) -> bool {
    if (const auto enabled = node.override_of(
            composition::HoverEffectEnvironment::is_hover_effect_enabled);
        enabled.has_value() && !enabled->get()) {
      return true;
    }
    return std::ranges::any_of(
        node.children(),
        [&self](const composition::Node& child) { return self(self, child); });
  };
  provider::headless::HeadlessProviderFactory providers;
  Application plain =
      start(providers, structure::List(structure::Section(
                           components::Text("Alpha").list_row_hover_effect(
                               motion::HoverEffect::lift))));
  EXPECT_FALSE(disabled(disabled, plain.root().tree().root()));
  provider::headless::HeadlessProviderFactory quiet_providers;
  Application quiet = start(
      quiet_providers, structure::List(structure::Section(
                           components::Text("Alpha")
                               .list_row_hover_effect(motion::HoverEffect::lift)
                               .list_row_hover_effect_disabled())));
  EXPECT_TRUE(disabled(disabled, quiet.root().tree().root()));
}

TEST_F(ListRowTest, BadgesShowTheirLabelAndZeroHidesIt) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, Names{std::make_shared<int>(0)});
  EXPECT_TRUE(shows(application, "5"));
  EXPECT_FALSE(shows(application, "0"));
}

TEST_F(ListRowTest, PlusOpensTheContextMenuAndShowsInTheHintBar) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, Names{std::make_shared<int>(0)});
  run(providers, application, 4);
  EXPECT_TRUE(hinted(application, Button::plus, "Options"));
  EXPECT_TRUE(hinted(application, Button::y, "Actions"));
  EXPECT_FALSE(shows(application, "Pin"));
  press(providers, application, Button::plus);
  run(providers, application, 30);
  EXPECT_TRUE(shows(application, "Pin"));
  EXPECT_TRUE(shows(application, "Erase"));
}

TEST_F(ListRowTest, HoldingAOnAnActivatableRowOpensTheMenuWithoutActivating) {
  provider::headless::HeadlessProviderFactory providers;
  auto opened = std::make_shared<int>(0);
  Application application = start(providers, Names{opened});
  press(providers, application, Button::dpad_down);
  press(providers, application, Button::dpad_down);
  run(providers, application, 6);
  EXPECT_TRUE(hinted(application, Button::a, "OK"));
  press(providers, application, Button::a);
  run(providers, application, 6);
  EXPECT_EQ(*opened, 1);
  hold_button(providers, application, Button::a,
              std::chrono::milliseconds{700});
  run(providers, application, 30);
  EXPECT_EQ(*opened, 1);
  EXPECT_TRUE(shows(application, "Pin"));
}

TEST_F(ListRowTest, YRevealsSwipeActionsAndAPerformsThem) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, Names{std::make_shared<int>(0)});
  run(providers, application, 4);
  EXPECT_FALSE(shows(application, "Delete"));
  press(providers, application, Button::y);
  run(providers, application, 30);
  EXPECT_TRUE(shows(application, "Delete"));
  EXPECT_TRUE(hinted(application, Button::y, "Close"));
  EXPECT_TRUE(hinted(application, Button::a, "Delete"));
  press(providers, application, Button::a);
  run(providers, application, 40);
  EXPECT_FALSE(shows(application, "Alpha"));
  EXPECT_TRUE(shows(application, "Bravo"));
}

TEST_F(ListRowTest, DraggingARowOpensItsSwipeActionsByTouch) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, Names{std::make_shared<int>(0)});
  run(providers, application, 4);
  const provider::PixelPoint from = centre(application, "Bravo");
  drag(providers, application, from,
       provider::PixelPoint{.x = from.x - 140.0F, .y = from.y}, 12);
  run(providers, application, 30);
  EXPECT_TRUE(shows(application, "Delete"));
  const provider::PixelPoint again = centre(application, "Delta");
  drag(providers, application, again,
       provider::PixelPoint{.x = again.x - 1000.0F, .y = again.y}, 16);
  run(providers, application, 40);
  EXPECT_FALSE(shows(application, "Delta"));
}

TEST_F(ListRowTest, MultiSelectionInEditModeBatchDeletesFromAThousandRows) {
  provider::headless::HeadlessProviderFactory providers;
  auto handle = std::make_shared<
      std::optional<composition::StateHandle<std::set<int>>>>();
  auto all = std::make_shared<
      std::optional<composition::StateHandle<std::vector<int>>>>();
  Application application = start(providers, Picks{handle, all});
  run(providers, application, 6);
  ASSERT_TRUE(handle->has_value());
  EXPECT_TRUE(hinted(application, Button::a, "Select"));
  press(providers, application, Button::a);
  press(providers, application, Button::dpad_down);
  press(providers, application, Button::a);
  press(providers, application, Button::dpad_down);
  press(providers, application, Button::a);
  press(providers, application, Button::dpad_down);
  press(providers, application, Button::a);
  run(providers, application, 6);
  const std::set<int> picked = (*handle)->get();
  EXPECT_EQ(picked, (std::set<int>{0, 1, 3}));
  tap(providers, application, centre(application, "arisa.list.select"));
  run(providers, application, 6);
  EXPECT_EQ((*handle)->get().size(), 2U);
  std::set<int> every;
  for (int row = 0; row < 1000; row += 2) {
    every.insert(row);
  }
  (*handle)->set(every);
  run(providers, application, 6);
  const std::set<int> gone = (*handle)->get();
  (*all)->update([&gone](std::vector<int>& rows) {
    std::erase_if(rows, [&gone](int row) { return gone.contains(row); });
  });
  (*handle)->set(std::set<int>{});
  run(providers, application, 30);
  EXPECT_EQ((*all)->get().size(), 500U);
  EXPECT_TRUE(shows(application, "Row 1"));
  EXPECT_FALSE(shows(application, "Row 0"));
}

TEST_F(ListRowTest, DisclosureGroupExpandsOnA) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers, components::VStack(structure::DisclosureGroup("Display", [] {
        return Element{components::Text("Child")};
      })));
  run(providers, application, 4);
  EXPECT_FALSE(shows(application, "Child"));
  press(providers, application, Button::a);
  run(providers, application, 30);
  EXPECT_TRUE(shows(application, "Child"));
  press(providers, application, Button::a);
  run(providers, application, 40);
  EXPECT_FALSE(shows(application, "Child"));
}

TEST_F(ListRowTest, OutlineListStartsCollapsedAndExpandsNodes) {
  struct Node {
    std::string name;
    std::optional<std::vector<Node>> children;
  };
  const std::vector<Node> tree{
      Node{.name = "Root",
           .children = std::vector<Node>{Node{.name = "Leaf",
                                              .children = std::nullopt}}},
      Node{.name = "Loose", .children = std::nullopt}};
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers,
            structure::List(
                tree, [](const Node& node) { return node.name; },
                [](const Node& node) { return node.children; },
                [](const Node& node) { return components::Text(node.name); }));
  run(providers, application, 4);
  EXPECT_TRUE(shows(application, "Root"));
  EXPECT_TRUE(shows(application, "Loose"));
  EXPECT_FALSE(shows(application, "Leaf"));
  press(providers, application, Button::a);
  run(providers, application, 30);
  EXPECT_TRUE(shows(application, "Leaf"));
}

TEST_F(ListRowTest, SectionIndexJumpsToTheSection) {
  std::vector<Element> sections;
  for (int letter = 0; letter < 26; ++letter) {
    const std::string label(1, static_cast<char>('A' + letter));
    sections.push_back(
        structure::Section(
            label, components::ForEach(
                       std::views::iota(0, 20),
                       [letter](int row) { return (letter * 100) + row; },
                       [label](int row) {
                         return components::Text(
                             std::format("{} {}", label, row));
                       }))
            .section_index_label(label));
  }
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers,
      structure::List(std::move(sections))
          .list_style(structure::PlainListStyle{})
          .list_section_index_visibility(composition::Visibility::visible));
  run(providers, application, 6);
  ASSERT_TRUE(node_of(application, "arisa.list.index").has_value());
  const composition::Rect rail = presented_of(application, "arisa.list.index");
  tap(providers, application,
      provider::PixelPoint{.x = rail.x + (rail.width * 0.5F),
                           .y = rail.y + (12.5F * 16.0F)});
  run(providers, application, 90);
  EXPECT_TRUE(shows(application, "M 0"));
  const composition::Rect section =
      presented_of(application, "arisa.list.section.12");
  EXPECT_NEAR(section.y, 0.0F, 40.0F);
}

TEST_F(ListRowTest, PinnedHeadersStayAtTheTopOfTheirSection) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, pinned_list());
  run(providers, application, 6);
  const composition::Rect scroll = presented_of(application, "pinned-scroll");
  const composition::Rect before = presented_of(application, "header-0");
  EXPECT_NEAR(before.y, scroll.y, 1.0F);
  drag(providers, application,
       provider::PixelPoint{.x = scroll.x + 200.0F, .y = scroll.y + 280.0F},
       provider::PixelPoint{.x = scroll.x + 200.0F, .y = scroll.y + 80.0F}, 10);
  settle(providers, application);
  EXPECT_GT(offset(application, "pinned-scroll").y, 100.0F);
  const composition::Rect pinned = presented_of(application, "header-0");
  EXPECT_NEAR(pinned.y, scroll.y, 1.0F);
}

TEST_F(ListRowTest, PlainListSectionHeadersStickWhileTheirSectionScrolls) {
  provider::headless::HeadlessProviderFactory providers;
  std::vector<Element> sections;
  for (int section = 0; section < 3; ++section) {
    std::vector<Element> rows;
    for (int row = 0; row < 15; ++row) {
      rows.push_back(components::Text(std::format("Row {} {}", section, row)));
    }
    sections.push_back(structure::Section(
        structure::Section::Labels{
            .header = components::Text(std::format("Section {}", section))
                          .id(std::format("plain-header-{}", section)),
            .footer = std::nullopt},
        std::move(rows)));
  }
  Application application =
      start(providers, structure::List(std::move(sections))
                           .list_style(structure::PlainListStyle{})
                           .frame({.width = 400.0F, .height = 300.0F})
                           .id("plain-list"));
  run(providers, application, 6);
  const composition::Rect list = presented_of(application, "plain-list");
  const float first = presented_of(application, "plain-header-0").y;
  const float second = presented_of(application, "plain-header-1").y;
  drag(providers, application,
       provider::PixelPoint{.x = list.x + 200.0F, .y = list.y + 280.0F},
       provider::PixelPoint{.x = list.x + 200.0F, .y = list.y + 80.0F}, 10,
       false);
  step(providers, application, std::chrono::milliseconds{200});
  touch_up(providers, application,
           provider::PixelPoint{.x = list.x + 200.0F, .y = list.y + 80.0F});
  settle(providers, application);
  const composition::Rect pinned = presented_of(application, "plain-header-0");
  EXPECT_NEAR(pinned.y, first, 2.0F);
  EXPECT_LT(presented_of(application, "plain-header-1").y, second - 150.0F);
}

TEST_F(ListRowTest, GridTilesReorderByController) {
  provider::headless::HeadlessProviderFactory providers;
  auto seen = std::make_shared<std::vector<int>>();
  Application application = start(providers, Tiles{seen});
  run(providers, application, 6);
  EXPECT_TRUE(hinted(application, Button::x, "Move"));
  press(providers, application, Button::x);
  run(providers, application, 4);
  EXPECT_TRUE(hinted(application, Button::x, "Drop"));
  press(providers, application, Button::dpad_right);
  run(providers, application, 20);
  EXPECT_EQ(*seen, (std::vector<int>{2, 1, 3, 4, 5, 6, 7, 8, 9}));
  press(providers, application, Button::dpad_down);
  run(providers, application, 20);
  EXPECT_EQ(*seen, (std::vector<int>{2, 3, 4, 5, 1, 6, 7, 8, 9}));
  press(providers, application, Button::a);
  run(providers, application, 6);
  EXPECT_TRUE(hinted(application, Button::x, "Move"));
}

TEST_F(ListRowTest, GroupedAndInsetStylesPlaceRows) {
  for (const auto& [style, margin] :
       {std::pair{structure::ListStyle::grouped(), 0.0F},
        std::pair{structure::ListStyle::inset(), 20.0F}}) {
    provider::headless::HeadlessProviderFactory providers;
    Application application = start(
        providers,
        structure::List(structure::Section(
                            components::VStack()
                                .frame({.height = 20.0F, .max_width = infinity})
                                .id("a")))
            .list_style(style));
    run(providers, application, 4);
    const composition::Rect text = presented_of(application, "a");
    EXPECT_GE(text.x, margin) << static_cast<int>(style.kind());
    EXPECT_LE(text.x, margin + 24.0F) << static_cast<int>(style.kind());
  }
}

TEST_F(ListRowTest, DeleteDisabledRowsOfferNoDeleteCommand) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, Editable{});
  run(providers, application, 6);
  EXPECT_FALSE(hinted(application, Button::minus, "Delete"));
  EXPECT_FALSE(hinted(application, Button::x, "Move"));
  press(providers, application, Button::dpad_down);
  run(providers, application, 4);
  EXPECT_TRUE(hinted(application, Button::minus, "Delete"));
  press(providers, application, Button::minus);
  run(providers, application, 40);
  EXPECT_FALSE(shows(application, "Item 2"));
  EXPECT_TRUE(shows(application, "Item 1"));
}

TEST_F(ListRowTest, PinnedHeaderPaintsOverTheCellsScrolledUnderIt) {
  provider::headless::HeadlessProviderFactory providers;
  const provider::SurfaceSize size{.width = provider::SurfaceWidth{400},
                                   .height = provider::SurfaceHeight{300}};
  providers.surface().resize(size);
  providers.gpu().rasterize(size);
  Application application = start(providers, pinned_list());
  run(providers, application, 6);
  const composition::Rect scroll = presented_of(application, "pinned-scroll");
  drag(providers, application,
       provider::PixelPoint{.x = scroll.x + 200.0F, .y = scroll.y + 280.0F},
       provider::PixelPoint{.x = scroll.x + 200.0F, .y = scroll.y + 60.0F}, 10);
  settle(providers, application);
  run(providers, application, 4);
  const auto image = providers.gpu().capture();
  ASSERT_TRUE(image.has_value());
  const auto red = [&image](float x, float y) {
    const auto seen = image->pixel(static_cast<std::uint32_t>(x),
                                   static_cast<std::uint32_t>(y));
    return seen.has_value() && (*seen)[0] > 180 && (*seen)[1] < 120 &&
           (*seen)[2] < 120;
  };
  EXPECT_TRUE(red(scroll.x + 380.0F, scroll.y + 4.0F));
  EXPECT_TRUE(red(scroll.x + 380.0F, scroll.y + 26.0F));
  EXPECT_FALSE(red(scroll.x + 380.0F, scroll.y + 40.0F));
}

const composition::StateKey<std::vector<int>> ListRowTest::Editable::items{
    "editable-items", std::vector<int>{1, 2, 3}};

}
