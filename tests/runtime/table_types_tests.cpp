#include <arisa/components/for_each.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/section_collection.hpp>
#include <arisa/components/section_configuration.hpp>
#include <arisa/components/text.hpp>
#include <arisa/composition/badge_prominence.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/edge_lengths.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/list_section_spacing.hpp>
#include <arisa/composition/sidebar_row_size.hpp>
#include <arisa/composition/visibility.hpp>
#include <arisa/structure/disclosure_table_row.hpp>
#include <arisa/structure/empty_table_row_content.hpp>
#include <arisa/structure/key_path_comparator.hpp>
#include <arisa/structure/list_style.hpp>
#include <arisa/structure/section.hpp>
#include <arisa/structure/sort_order.hpp>
#include <arisa/structure/tab_section_expansion.hpp>
#include <arisa/structure/tab_view_customization.hpp>
#include <arisa/structure/table.hpp>
#include <arisa/structure/table_column.hpp>
#include <arisa/structure/table_column_alignment.hpp>
#include <arisa/structure/table_column_customization.hpp>
#include <arisa/structure/table_column_customization_behavior.hpp>
#include <arisa/structure/table_column_for_each.hpp>
#include <arisa/structure/table_for_each_content.hpp>
#include <arisa/structure/table_header_row_content.hpp>
#include <arisa/structure/table_outline_group_content.hpp>
#include <arisa/structure/table_row.hpp>
#include <arisa/structure/table_style.hpp>
#include <arisa/structure/tuple_table_column_content.hpp>
#include <arisa/structure/tuple_table_row_content.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace arisa::runtime {

using composition::Element;

class TableTypesTest : public ::testing::Test {
 protected:
  struct Item {
    int id{0};
    std::string name;
    int size{0};
    std::optional<std::vector<Item>> children;
  };

  template <class... Views>
  static auto elements(Views&&... views) -> std::vector<Element> {
    std::vector<Element> made;
    (made.push_back(Element{std::forward<Views>(views)}), ...);
    return made;
  }

  static auto item(int id, std::string name, int size) -> Item {
    return Item{.id = id,
                .name = std::move(name),
                .size = size,
                .children = std::nullopt};
  }
};

TEST_F(TableTypesTest, CustomizationBehaviorIsASetAlgebra) {
  using Behavior = structure::TableColumnCustomizationBehavior;
  Behavior none;
  EXPECT_TRUE(none.empty());
  EXPECT_TRUE(Behavior::all.contains(Behavior::resize));
  EXPECT_TRUE(Behavior::all.contains(Behavior::reorder));
  EXPECT_TRUE(Behavior::all.contains(Behavior::visibility));
  const Behavior both = Behavior::resize.union_with(Behavior::visibility);
  EXPECT_TRUE(both.contains(Behavior::visibility));
  EXPECT_FALSE(both.contains(Behavior::reorder));
  EXPECT_EQ(both.intersection(Behavior::resize), Behavior::resize);
  EXPECT_EQ(both.symmetric_difference(Behavior::resize), Behavior::visibility);
  Behavior edited;
  EXPECT_TRUE(edited.insert(Behavior::reorder).first);
  EXPECT_FALSE(edited.insert(Behavior::reorder).first);
  EXPECT_EQ(edited.update(Behavior::reorder), Behavior::reorder);
  EXPECT_FALSE(edited.update(Behavior::resize).has_value());
  EXPECT_EQ(edited.remove(Behavior::resize), Behavior::resize);
  EXPECT_FALSE(edited.remove(Behavior::visibility).has_value());
  edited.form_union(Behavior::visibility);
  EXPECT_TRUE(edited.contains(Behavior::visibility));
  edited.form_intersection(Behavior::visibility);
  EXPECT_EQ(edited, Behavior::visibility);
  edited.form_symmetric_difference(Behavior::all);
  EXPECT_EQ(edited, Behavior::resize.union_with(Behavior::reorder));
}

TEST_F(TableTypesTest, ColumnAlignmentsResolve) {
  using Alignment = structure::TableColumnAlignment;
  EXPECT_EQ(Alignment::automatic.resolved(), Alignment::Kind::leading);
  EXPECT_EQ(Alignment::leading.resolved(), Alignment::Kind::leading);
  EXPECT_EQ(Alignment::center.resolved(), Alignment::Kind::center);
  EXPECT_EQ(Alignment::trailing.resolved(), Alignment::Kind::trailing);
  EXPECT_EQ(Alignment::numeric.resolved(), Alignment::Kind::trailing);
  const Alignment arabic = Alignment::numeric_in("arab");
  EXPECT_EQ(arabic.kind(), Alignment::Kind::numeric);
  EXPECT_EQ(arabic.numbering_system(), "arab");
  EXPECT_FALSE(arabic == Alignment::numeric);
  EXPECT_TRUE(Alignment::numeric == Alignment::numeric);
}

TEST_F(TableTypesTest, ColumnCustomizationStoresVisibilityOrderAndWidth) {
  structure::TableColumnCustomization customization;
  EXPECT_EQ(customization, structure::TableColumnCustomization{});
  EXPECT_EQ(customization.visibility("name"),
            composition::Visibility::automatic);
  customization.set_visibility("kind", composition::Visibility::hidden);
  customization.set_width("name", 120.0F);
  customization.reorder({"size", "name"});
  const std::vector<std::string> declared{"name", "kind", "size"};
  EXPECT_EQ(customization.arranged(declared),
            (std::vector<std::size_t>{2, 0, 1}));
  EXPECT_EQ(customization.visibility("kind"), composition::Visibility::hidden);
  EXPECT_EQ(customization.width("name"), 120.0F);
  const std::optional<structure::TableColumnCustomization> decoded =
      structure::TableColumnCustomization::decoded(customization.encoded());
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(*decoded, customization);
  customization.reset_order();
  EXPECT_EQ(customization.arranged(declared),
            (std::vector<std::size_t>{0, 1, 2}));
  EXPECT_FALSE(decoded == customization);
}

TEST_F(TableTypesTest, TableStylesCarryAlternation) {
  EXPECT_EQ(structure::TableStyle{structure::AutomaticTableStyle{}},
            structure::TableStyle::automatic());
  EXPECT_EQ(structure::TableStyle{structure::InsetTableStyle{}}.kind(),
            structure::TableStyle::Kind::inset);
  EXPECT_FALSE(structure::TableStyle{structure::InsetTableStyle{false}}
                   .alternates_row_backgrounds());
  EXPECT_EQ(structure::TableStyle{structure::BorderedTableStyle{}},
            structure::TableStyle::bordered());
  EXPECT_FALSE(structure::TableStyle{structure::BorderedTableStyle{false}}
                   .alternates_row_backgrounds());
  EXPECT_EQ(structure::TableStyle::bordered(false).kind(),
            structure::TableStyle::Kind::bordered);
  EXPECT_EQ(structure::TableStyleConfiguration{},
            structure::TableStyle::Configuration{});
}

TEST_F(TableTypesTest, ComparatorsSortByTheirKeyAndOrder) {
  structure::KeyPathComparator<Item> by_name{&Item::name};
  const Item alpha = item(1, "Alpha", 3);
  const Item bravo = item(2, "Bravo", 1);
  EXPECT_TRUE(by_name(alpha, bravo));
  const structure::KeyPathComparator<Item> reversed = by_name.reversed();
  EXPECT_EQ(reversed.order(), structure::SortOrder::reverse);
  EXPECT_TRUE(reversed(bravo, alpha));
  EXPECT_TRUE(reversed.same_key(by_name));
  EXPECT_FALSE(reversed == by_name);
  const structure::KeyPathComparator<Item> by_size{
      &Item::size, [](int left, int right) { return left > right; }};
  EXPECT_FALSE(by_size.same_key(by_name));
  EXPECT_TRUE(by_size(alpha, bravo));
}

TEST_F(TableTypesTest, ColumnsDescribeTitleSortAndWidth) {
  const structure::TableColumn<Item> plain{
      "Plain", [](const Item& row) { return components::Text(row.name); }};
  EXPECT_EQ(plain.specs().front().title, "Plain");
  EXPECT_FALSE(plain.specs().front().comparator.has_value());
  const structure::TableColumn<Item> labelled{
      components::Text("Label"),
      [](const Item& row) { return components::Text(row.name); }};
  EXPECT_EQ(labelled.specs().front().title, "Label");
  EXPECT_NE(labelled.specs().front().label, nullptr);
  const structure::TableColumn<Item> sorting{
      "Sorting", structure::KeyPathComparator<Item>{&Item::size},
      [](const Item& row) { return components::Text(row.name); }};
  EXPECT_TRUE(sorting.specs().front().comparator.has_value());
  const structure::TableColumn<Item> valued{"Name", &Item::name};
  EXPECT_TRUE(valued.specs().front().comparator.has_value());
  const structure::TableColumn<Item> compared{
      "Name", &Item::name,
      [](const std::string& left, const std::string& right) {
        return left < right;
      }};
  EXPECT_TRUE(compared.specs().front().comparator.has_value());
  const structure::TableColumn<Item> sized{
      "Size", &Item::size, [](const Item& row) {
        return components::Text(std::to_string(row.size));
      }};
  EXPECT_TRUE(sized.specs().front().comparator.has_value());
  structure::TableColumn<Item> full{
      "Size", &Item::size, [](int left, int right) { return left < right; },
      [](const Item& row) {
        return components::Text(std::to_string(row.size));
      }};
  full = std::move(full).width(80.0F);
  EXPECT_EQ(full.specs().front().width.fixed, 80.0F);
  full = std::move(full).width(40.0F, 60.0F, 90.0F);
  EXPECT_EQ(full.specs().front().width.min, 40.0F);
  EXPECT_EQ(full.specs().front().width.ideal, 60.0F);
  EXPECT_EQ(full.specs().front().width.max, 90.0F);
  full = std::move(full)
             .customization_id("size")
             .default_visibility(composition::Visibility::hidden)
             .disabled_customization_behavior(
                 structure::TableColumnCustomizationBehavior::resize)
             .alignment(structure::TableColumnAlignment::trailing);
  EXPECT_EQ(full.specs().front().customization, "size");
  EXPECT_EQ(full.specs().front().default_visibility,
            composition::Visibility::hidden);
  EXPECT_TRUE(full.specs().front().disabled.contains(
      structure::TableColumnCustomizationBehavior::resize));
  EXPECT_EQ(full.specs().front().alignment,
            structure::TableColumnAlignment::trailing);
}

TEST_F(TableTypesTest, ColumnForEachAndTuplesExpandColumns) {
  const std::vector<std::string> names{"a", "b", "c"};
  const structure::TableColumnForEach<Item> each{
      names, [](const std::string& name) { return name; },
      [](const std::string& name) {
        return structure::TableColumn<Item>(
            name, [](const Item& row) { return components::Text(row.name); });
      }};
  EXPECT_EQ(each.content().size(), 3U);
  EXPECT_EQ(each.data().size(), 3U);
  EXPECT_EQ(each.content()[1].customization, "b");
  const structure::TableColumnForEach<Item> counted{
      2, [](int index) {
        return structure::TableColumn<Item>(
            std::to_string(index),
            [](const Item& row) { return components::Text(row.name); });
      }};
  EXPECT_EQ(counted.content().size(), 2U);
  const structure::TupleTableColumnContent<Item> tuple{
      structure::TableColumn<Item>("Name", &Item::name), counted};
  EXPECT_EQ(tuple.value().size(), 3U);
  EXPECT_EQ(tuple.value().front().title, "Name");
}

TEST_F(TableTypesTest, RowContentBuildsNodes) {
  const structure::TableRow<Item> row{item(1, "One", 1)};
  ASSERT_EQ(row.table_row_body().size(), 1U);
  EXPECT_EQ(row.table_row_body().front().kind, structure::TableRowKind::row);
  const structure::DisclosureTableRow<Item> disclosed{
      item(2, "Two", 2), structure::TableRow<Item>{item(3, "Three", 3)}};
  ASSERT_EQ(disclosed.table_row_body().size(), 1U);
  EXPECT_EQ(disclosed.table_row_body().front().kind,
            structure::TableRowKind::disclosure);
  EXPECT_EQ(disclosed.table_row_body().front().children.size(), 1U);
  const structure::TableHeaderRowContent<Item> header{
      "Header", structure::TableRow<Item>{item(4, "Four", 4)},
      structure::EmptyTableRowContent<Item>{}};
  EXPECT_EQ(header.table_row_body().front().kind,
            structure::TableRowKind::header);
  EXPECT_EQ(header.table_row_body().front().children.size(), 1U);
  const structure::TupleTableRowContent<Item> tuple{row, disclosed, header};
  EXPECT_EQ(tuple.value().size(), 3U);
  const std::vector<Item> items{item(5, "Five", 5), item(6, "Six", 6)};
  structure::TableForEachContent<Item> each{items};
  EXPECT_EQ(each.data(), 2U);
  std::size_t inserted = 99;
  each = std::move(each).on_insert(
      {composition::UTType::plain_text()},
      [&inserted](std::size_t at, std::vector<composition::TransferItem>) {
        inserted = at;
      });
  ASSERT_NE(each.table_row_body()[1].insert, nullptr);
  each.table_row_body()[1].insert->action(each.table_row_body()[1].insert_index,
                                          {});
  EXPECT_EQ(inserted, 1U);
  static_assert(
      structure::DynamicTableRowContent<structure::TableForEachContent<Item>>);
  const structure::TableForEachContent<Item> built{
      items, [](const Item& value) {
        return structure::DisclosureTableRow<Item>{value};
      }};
  EXPECT_EQ(built.table_row_body().front().kind,
            structure::TableRowKind::disclosure);
  std::vector<Item> tree{item(7, "Seven", 7)};
  tree.front().children = std::vector<Item>{item(8, "Eight", 8)};
  const structure::TableOutlineGroupContent<Item> outline{
      tree, [](const Item& value) { return value.children; }};
  EXPECT_EQ(outline.table_row_body().front().kind,
            structure::TableRowKind::disclosure);
  EXPECT_EQ(outline.table_row_body().front().children.size(), 1U);
}

TEST_F(TableTypesTest, ListValueTypesCompare) {
  EXPECT_NE(composition::BadgeProminence::decreased,
            composition::BadgeProminence::increased);
  EXPECT_EQ(composition::BadgeProminence::standard,
            composition::BadgeProminence::standard);
  EXPECT_FALSE(composition::ListSectionSpacing::default_.length().has_value());
  EXPECT_EQ(composition::ListSectionSpacing::compact.length(),
            composition::ListSectionSpacing::compact_length);
  EXPECT_EQ(composition::ListSectionSpacing::custom(12.0F).length(), 12.0F);
  composition::EdgeLengths lengths;
  lengths.set(theme::Edge::horizontal, 4.0F);
  EXPECT_EQ(lengths.leading, 4.0F);
  EXPECT_EQ(lengths.trailing, 4.0F);
  EXPECT_FALSE(lengths.top.has_value());
  EXPECT_NE(structure::TabSectionExpansion::collapsed,
            structure::TabSectionExpansion::expanded);
  EXPECT_NE(composition::SidebarRowSize::small,
            composition::SidebarRowSize::large);
  EXPECT_EQ(structure::ListStyle{structure::BorderedListStyle{true}},
            structure::ListStyle::bordered(true));
  EXPECT_NE(structure::ListStyle::bordered(),
            structure::ListStyle::bordered(true));
}

TEST_F(TableTypesTest, TabSectionCustomizationOrdersPerSection) {
  structure::TabViewCustomization customization;
  EXPECT_FALSE(customization.section("mail").tab_order().has_value());
  customization.reorder_section("mail", {"sent", "inbox"});
  EXPECT_EQ(customization.section_order("mail"),
            (std::vector<std::string>{"sent", "inbox"}));
  structure::TabViewCustomization::SectionCustomization section =
      customization.section("mail");
  EXPECT_EQ(section.tab_order(), (std::vector<std::string>{"sent", "inbox"}));
  section.reset_tab_order();
  EXPECT_FALSE(section.tab_order().has_value());
  customization.set_section("mail", section);
  EXPECT_FALSE(customization.section_order("mail").has_value());
  customization.reorder_section("mail", {"b", "a"});
  customization.reorder_section("news", {"c"});
  const std::optional<structure::TabViewCustomization> decoded =
      structure::TabViewCustomization::decoded(customization.encoded());
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(*decoded, customization);
  customization.reset_section_order("news");
  EXPECT_FALSE(customization.section_order("news").has_value());
  EXPECT_TRUE(customization.section_order("mail").has_value());
  customization.reset_section_order();
  EXPECT_FALSE(customization.section_order("mail").has_value());
}

TEST_F(TableTypesTest, SectionCollectionsReadSections) {
  components::SectionCollection sections{elements(
      components::Text("Loose"),
      structure::Section(
          structure::Section::Labels{.header = components::Text("Header"),
                                     .footer = components::Text("Footer")},
          components::Text("One"), components::Text("Two"))
          .section_actions([] { return Element{components::Text("Act")}; })
          .id("titled"),
      structure::Section(components::ForEach(
          std::vector<int>{1, 2, 3}, [](int value) { return value; },
          [](int value) { return components::Text(std::to_string(value)); })))};
  ASSERT_EQ(sections.size(), 3U);
  EXPECT_TRUE(sections[0].header().empty());
  EXPECT_EQ(sections[0].content().size(), 1U);
  EXPECT_EQ(sections[1].id(), components::SectionConfiguration::ID{"titled"});
  EXPECT_EQ(sections[1].header().size(), 1U);
  EXPECT_EQ(sections[1].footer().size(), 1U);
  EXPECT_EQ(sections[1].content().size(), 2U);
  EXPECT_EQ(sections[1].actions().size(), 1U);
  EXPECT_EQ(sections[2].content().size(), 3U);
  EXPECT_FALSE(sections[2].id() == sections[1].id());
  EXPECT_TRUE(sections[2].container_values().empty());
  const std::vector<Element> grouped = components::Group::members(Element{
      components::Group(components::SectionCollection{elements(
                            structure::Section(components::Text("A")),
                            structure::Section(components::Text("B")))},
                        [](components::SectionCollection found) {
                          return components::Text(std::to_string(found.size()));
                        })});
  EXPECT_EQ(grouped.size(), 1U);
  const std::vector<Element> each =
      components::Group::members(Element{components::ForEach(
          components::SectionCollection{
              elements(structure::Section(components::Text("A")),
                       structure::Section(components::Text("B")))},
          [](components::SectionConfiguration section) {
            return components::Text(std::to_string(section.content().size()));
          })});
  EXPECT_EQ(each.size(), 2U);
}

}
