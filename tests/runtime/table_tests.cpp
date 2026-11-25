#include <arisa/components/button.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/user_interface_size_class.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/transfer_item.hpp>
#include <arisa/composition/visibility.hpp>
#include <arisa/controls/context_menu.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/structure/disclosure_table_row.hpp>
#include <arisa/structure/key_path_comparator.hpp>
#include <arisa/structure/table.hpp>
#include <arisa/structure/table_column.hpp>
#include <arisa/structure/table_column_customization.hpp>
#include <arisa/structure/table_for_each_content.hpp>
#include <arisa/structure/table_header_row_content.hpp>
#include <arisa/structure/table_row.hpp>
#include <arisa/structure/table_style.hpp>
#include <arisa/structure/tuple_table_row_content.hpp>

#include <scroll_fixture.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace arisa::runtime {

using composition::Element;
using provider::Button;
using structure::KeyPathComparator;
using structure::TableColumn;

class TableTest : public ScrollFixture {
 protected:
  struct Item {
    int id{0};
    std::string name;
    std::string kind;
    int size{0};
    std::optional<std::vector<Item>> children;
  };

  using Body = std::function<Element(composition::Context&)>;

  class Host final : public composition::Component {
   public:
    explicit Host(std::shared_ptr<const Body> body) noexcept
        : body_(std::move(body)) {}

    auto operator==(const Host& other) const -> bool {
      return body_ == other.body_;
    }

    auto body(composition::Context& context) const -> Element override {
      return (*body_)(context);
    }

   private:
    std::shared_ptr<const Body> body_;
  };

  static auto host(Body body) -> Element {
    return Host{std::make_shared<const Body>(std::move(body))};
  }

  static auto items(int count) -> std::vector<Item> {
    std::vector<Item> made;
    for (int index = 0; index < count; ++index) {
      made.push_back(Item{.id = index,
                          .name = std::format("Item {}", index),
                          .kind = index % 2 == 0 ? "Even" : "Odd",
                          .size = (index * 37) % 100,
                          .children = std::nullopt});
    }
    return made;
  }

  static void run(provider::headless::HeadlessProviderFactory& providers,
                  Application& application, int count) {
    for (int index = 0; index < count; ++index) {
      ScrollFixture::step(providers, application);
    }
  }

  static auto lines(const composition::Node& node) -> std::size_t {
    std::size_t found =
        node.content_as<structure::Table::Line::Content>().has_value() ? 1 : 0;
    for (const composition::Node& child : node.children()) {
      found += lines(child);
    }
    return found;
  }

  static auto line_inside(const composition::Node& node)
      -> const composition::Node* {
    if (node.content_as<structure::Table::Line::Content>().has_value()) {
      return &node;
    }
    for (const composition::Node& child : node.children()) {
      if (const composition::Node* found = line_inside(child)) {
        return found;
      }
    }
    return nullptr;
  }

  static auto columns() {
    return std::make_tuple(
        TableColumn<Item>("Name", &Item::name),
        TableColumn<Item>("Kind", &Item::kind),
        TableColumn<Item>("Size", &Item::size, [](const Item& row) {
          return components::Text(std::to_string(row.size));
        }));
  }
};

TEST_F(TableTest, RowsRealiseLazilyWithMeasuredMetrics) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers, host([](composition::Context&) {
              auto [name, kind, size] = columns();
              return structure::Table(items(1000), &Item::id, std::move(name),
                                      std::move(kind), std::move(size))
                  .frame({.width = 360.0F, .height = 300.0F})
                  .id("table");
            }));
  run(providers, application, 6);
  EXPECT_TRUE(shows(application, "Item 0"));
  EXPECT_FALSE(shows(application, "Item 999"));
  EXPECT_LT(lines(application.root().tree().root()), 40U);
  const composition::Rect table = presented_of(application, "table");
  const composition::Rect headers =
      presented_of(application, "arisa.table.headers");
  EXPECT_NEAR(headers.height,
              structure::Table::header_height + structure::Table::header_gap,
              0.5F);
  const composition::Rect first = presented_of(application, "0");
  const composition::Rect second = presented_of(application, "1");
  EXPECT_NEAR(first.height, structure::Table::row_height, 0.5F);
  EXPECT_NEAR(second.y - first.y, structure::Table::row_height, 0.5F);
  EXPECT_NEAR(first.y - table.y, 33.0F, 1.0F);
  const float name_x = presented_of(application, "arisa.table.header.0").x;
  const float kind_x = presented_of(application, "arisa.table.header.1").x;
  const float size_x = presented_of(application, "arisa.table.header.2").x;
  EXPECT_NEAR(name_x - table.x, 18.5F, 1.5F);
  EXPECT_NEAR(kind_x - table.x, 130.5F, 2.5F);
  EXPECT_NEAR(size_x - table.x, 242.0F, 4.0F);
}

TEST_F(TableTest, HeadersSortByTouchAndController) {
  provider::headless::HeadlessProviderFactory providers;
  auto order = std::make_shared<std::optional<
      composition::StateHandle<std::vector<KeyPathComparator<Item>>>>>();
  static const composition::StateKey<std::vector<KeyPathComparator<Item>>>
      sorting{"table-test-sorting", {}};
  Application application =
      start(providers, host([order](composition::Context& context) {
              const auto by = context.state(sorting);
              *order = by;
              auto [name, kind, size] = columns();
              return structure::Table(items(5), &Item::id, by, std::move(name),
                                      std::move(kind), std::move(size));
            }));
  run(providers, application, 6);
  ASSERT_TRUE(order->has_value());
  tap(providers, application, centre(application, "arisa.table.header.0"));
  run(providers, application, 6);
  ASSERT_EQ((*order)->get().size(), 1U);
  EXPECT_EQ((*order)->get().front().order(), structure::SortOrder::forward);
  EXPECT_TRUE(node_of(application, "arisa.table.sort-indicator").has_value());
  tap(providers, application, centre(application, "arisa.table.header.0"));
  run(providers, application, 6);
  EXPECT_EQ((*order)->get().front().order(), structure::SortOrder::reverse);
  tap(providers, application, centre(application, "arisa.table.header.2"));
  run(providers, application, 6);
  ASSERT_EQ((*order)->get().size(), 2U);
  EXPECT_EQ((*order)->get().front().order(), structure::SortOrder::forward);
  press(providers, application, Button::a);
  run(providers, application, 6);
  EXPECT_EQ((*order)->get().front().order(), structure::SortOrder::reverse);
}

TEST_F(TableTest, SingleSelectionFollowsFocusAndColumnsSteer) {
  provider::headless::HeadlessProviderFactory providers;
  auto picked = std::make_shared<
      std::optional<composition::StateHandle<std::optional<int>>>>();
  static const composition::StateKey<std::optional<int>> chosen{
      "table-test-chosen", std::nullopt};
  Application application =
      start(providers, host([picked](composition::Context& context) {
              const auto selection = context.state(chosen);
              *picked = selection;
              return components::HStack(
                  structure::Table(items(10), &Item::id, selection,
                                   TableColumn<Item>("Name", &Item::name),
                                   TableColumn<Item>("Kind", &Item::kind))
                      .table_column_headers(composition::Visibility::hidden)
                      .frame({.width = 400.0F, .height = 300.0F}),
                  components::Button("Side", [] {}).id("side"));
            }));
  run(providers, application, 6);
  ASSERT_TRUE(picked->has_value());
  press(providers, application, Button::dpad_down);
  run(providers, application, 4);
  ASSERT_TRUE((*picked)->get().has_value());
  const int first = *(*picked)->get();
  press(providers, application, Button::dpad_down);
  run(providers, application, 4);
  EXPECT_EQ((*picked)->get(), first + 1);
  press(providers, application, Button::dpad_right);
  run(providers, application, 4);
  EXPECT_NE(focused_key(application), "side");
  EXPECT_EQ((*picked)->get(), first + 1);
  press(providers, application, Button::dpad_right);
  run(providers, application, 4);
  EXPECT_EQ(focused_key(application), "side");
  tap(providers, application, centre(application, "4"));
  run(providers, application, 4);
  EXPECT_EQ((*picked)->get(), 4);
}

TEST_F(TableTest, MultipleSelectionTogglesAndOffersASelectionMenu) {
  provider::headless::HeadlessProviderFactory providers;
  auto picked = std::make_shared<
      std::optional<composition::StateHandle<std::set<int>>>>();
  auto performed = std::make_shared<std::set<int>>();
  static const composition::StateKey<std::set<int>> chosen{"table-test-many",
                                                           {}};
  Application application = start(
      providers, host([picked, performed](composition::Context& context) {
        const auto selection = context.state(chosen);
        *picked = selection;
        return structure::Table(items(10), &Item::id, selection,
                                TableColumn<Item>("Name", &Item::name))
            .context_menu(
                std::type_identity<int>{},
                [](const std::set<int>& ids) {
                  return components::Button(std::format("Use {}", ids.size()),
                                            [] {});
                },
                [performed](const std::set<int>& ids) { *performed = ids; });
      }));
  run(providers, application, 6);
  tap(providers, application, centre(application, "2"));
  tap(providers, application, centre(application, "3"));
  run(providers, application, 4);
  EXPECT_EQ((*picked)->get(), (std::set<int>{2, 3}));
  tap(providers, application, centre(application, "3"));
  run(providers, application, 4);
  EXPECT_EQ((*picked)->get(), (std::set<int>{2}));
  press(providers, application, Button::dpad_down);
  run(providers, application, 4);
  press(providers, application, Button::plus);
  run(providers, application, 20);
  const bool menu = shows(application, "Use 2") || shows(application, "Use 1");
  EXPECT_TRUE(menu);
  press(providers, application, Button::b);
  run(providers, application, 20);
  press(providers, application, Button::a);
  run(providers, application, 4);
  EXPECT_FALSE(performed->empty());
}

TEST_F(TableTest, ColumnsHideReorderAndResize) {
  provider::headless::HeadlessProviderFactory providers;
  auto layout = std::make_shared<std::optional<
      composition::StateHandle<structure::TableColumnCustomization>>>();
  static const composition::StateKey<structure::TableColumnCustomization>
      arranged{"table-test-arranged", {}};
  Application application = start(
      providers, host([layout](composition::Context& context) {
        const auto customization = context.state(arranged);
        *layout = customization;
        return structure::Table(
                   items(4), &Item::id, customization,
                   TableColumn<Item>("Name", &Item::name)
                       .customization_id("name"),
                   TableColumn<Item>("Kind", &Item::kind)
                       .customization_id("kind"),
                   TableColumn<Item>("Size", &Item::size,
                                     [](const Item& row) {
                                       return components::Text(
                                           std::to_string(row.size));
                                     })
                       .customization_id("size")
                       .default_visibility(composition::Visibility::hidden))
            .frame({.width = 600.0F, .height = 300.0F});
      }));
  run(providers, application, 6);
  EXPECT_FALSE(node_of(application, "arisa.table.header.size").has_value());
  ASSERT_TRUE(layout->has_value());
  tap(providers, application, centre(application, "arisa.table.header.name"));
  run(providers, application, 4);
  press(providers, application, Button::plus);
  run(providers, application, 20);
  ASSERT_TRUE(node_of(application, "arisa.table.visibility.size").has_value());
  tap(providers, application,
      centre(application, "arisa.table.visibility.size"));
  run(providers, application, 30);
  EXPECT_EQ((*layout)->get().visibility("size"),
            composition::Visibility::visible);
  EXPECT_TRUE(node_of(application, "arisa.table.header.size").has_value());
  press(providers, application, Button::x);
  run(providers, application, 4);
  press(providers, application, Button::dpad_right);
  run(providers, application, 10);
  press(providers, application, Button::x);
  run(providers, application, 4);
  const std::vector<std::string> declared{"name", "kind", "size"};
  const std::vector<std::size_t> placed = (*layout)->get().arranged(declared);
  ASSERT_EQ(placed.size(), 3U);
  EXPECT_NE(placed.front(), 0U);
  const auto handle = node_of(application, "arisa.table.resize");
  ASSERT_TRUE(handle.has_value());
  const provider::PixelPoint from = centre(application, "arisa.table.resize");
  drag(providers, application, from,
       provider::PixelPoint{.x = from.x + 60.0F, .y = from.y}, 8);
  run(providers, application, 6);
  const bool resized = (*layout)->get().width("name").has_value() ||
                       (*layout)->get().width("kind").has_value() ||
                       (*layout)->get().width("size").has_value();
  EXPECT_TRUE(resized);
}

TEST_F(TableTest, DisclosureAndHeaderRowsExpand) {
  provider::headless::HeadlessProviderFactory providers;
  static constexpr composition::StateKey<bool> folded{"table-test-folded",
                                                      false};
  auto group =
      std::make_shared<std::optional<composition::StateHandle<bool>>>();
  Application application =
      start(providers, host([group](composition::Context& context) {
              const auto open = context.state(folded);
              *group = open;
              const std::vector<Item> all = items(6);
              return structure::Table(
                  std::type_identity<Item>{}, &Item::id,
                  TableColumn<Item>("Name", &Item::name),
                  structure::TupleTableRowContent<Item>(
                      structure::DisclosureTableRow<Item>(
                          all[0], structure::TableRow<Item>{all[1]}),
                      structure::TableHeaderRowContent<Item>(
                          "Group", open, structure::TableRow<Item>{all[2]}),
                      structure::TableHeaderRowContent<Item>(
                          "Open", structure::TableRow<Item>{all[3]})));
            }));
  run(providers, application, 6);
  EXPECT_TRUE(shows(application, "Item 0"));
  EXPECT_FALSE(shows(application, "Item 1"));
  EXPECT_TRUE(shows(application, "Group"));
  EXPECT_FALSE(shows(application, "Item 2"));
  EXPECT_TRUE(shows(application, "Item 3"));
  tap(providers, application, centre(application, "0"));
  run(providers, application, 30);
  EXPECT_TRUE(shows(application, "Item 1"));
  (*group)->set(true);
  run(providers, application, 30);
  EXPECT_TRUE(shows(application, "Item 2"));
}

TEST_F(TableTest, OutlineDataExpandsWithTheDPad) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers, host([](composition::Context&) {
        std::vector<Item> roots = items(2);
        roots[0].children = std::vector<Item>{Item{.id = 10,
                                                   .name = "Child",
                                                   .kind = "",
                                                   .size = 0,
                                                   .children = std::nullopt}};
        return structure::Table(
                   roots, &Item::id,
                   [](const Item& row) { return row.children; },
                   TableColumn<Item>("Name", &Item::name))
            .table_style(structure::BorderedTableStyle{});
      }));
  run(providers, application, 6);
  EXPECT_FALSE(shows(application, "Child"));
  press(providers, application, Button::dpad_down);
  run(providers, application, 4);
  press(providers, application, Button::dpad_right);
  run(providers, application, 30);
  EXPECT_TRUE(shows(application, "Child"));
  press(providers, application, Button::dpad_left);
  run(providers, application, 30);
  EXPECT_FALSE(shows(application, "Child"));
}

TEST_F(TableTest, HeadersHideAndCompactWidthsShowOneColumn) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers, host([](composition::Context&) {
        auto [name, kind, size] = columns();
        return components::HStack(
            structure::Table(items(3), &Item::id,
                             TableColumn<Item>("Name", &Item::name))
                .table_column_headers(composition::Visibility::hidden)
                .frame({.width = 300.0F, .height = 200.0F})
                .id("hidden"),
            structure::Table(items(3), &Item::id, std::move(name),
                             std::move(kind), std::move(size))
                .environment(components::Environment::horizontal_size_class,
                             components::UserInterfaceSizeClass::compact)
                .frame({.width = 300.0F, .height = 200.0F}));
      }));
  run(providers, application, 6);
  EXPECT_FALSE(node_of(application, "arisa.table.headers").has_value());
  EXPECT_TRUE(shows(application, "Item 1"));
  EXPECT_FALSE(shows(application, "Odd"));
}

TEST_F(TableTest, InsertedRowsAcceptDrops) {
  provider::headless::HeadlessProviderFactory providers;
  auto landed = std::make_shared<std::size_t>(99);
  Application application =
      start(providers, host([landed](composition::Context&) {
              return structure::Table(
                  std::type_identity<Item>{}, &Item::id,
                  TableColumn<Item>("Name", &Item::name),
                  structure::TableForEachContent<Item>(items(4)).on_insert(
                      {composition::UTType::plain_text()},
                      [landed](std::size_t at,
                               std::vector<composition::TransferItem>) {
                        *landed = at;
                      }));
            }));
  run(providers, application, 6);
  const auto row = node_named(application, "2");
  ASSERT_TRUE(row.has_value());
  const composition::Node* line = line_inside(row->get());
  ASSERT_NE(line, nullptr);
  ASSERT_NE(line->interactions().drop, nullptr);
  EXPECT_TRUE(line->interactions().drop->perform({}, composition::Point{}));
  EXPECT_EQ(*landed, 2U);
}

}
