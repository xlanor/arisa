#include <layout_fixture.hpp>

#include <arisa/components/divider.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/for_each.hpp>
#include <arisa/components/grid.hpp>
#include <arisa/components/grid_item.hpp>
#include <arisa/components/grid_row.hpp>
#include <arisa/components/grid_section.hpp>
#include <arisa/components/grid_tracks.hpp>
#include <arisa/components/lazy_hgrid.hpp>
#include <arisa/components/lazy_vgrid.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/modifiers.hpp>
#include <arisa/motion/unit_point.hpp>
#include <arisa/theme/theme.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <format>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace arisa::layout {

using components::GridItem;
using composition::infinity;

class GridLayoutTest : public ::testing::Test, protected LayoutFixture {
 protected:
  static constexpr float tolerance = 0.01F;

  static auto run(composition::Tree& target) -> LayoutContext {
    auto context = LayoutContext::create();
    EXPECT_TRUE(context.has_value());
    const auto ran = context->update(target, handheld, std::nullopt);
    EXPECT_TRUE(ran.has_value());
    return std::move(*context);
  }

  struct Expected {
    float x;
    float width;
    float y;
  };

  static auto cell(std::string_view key, float height = 20.0F)
      -> composition::Element {
    return components::VStack()
        .frame({.height = height, .max_width = infinity})
        .background(theme::ColourRole::accent, theme::Shape::rectangle())
        .id(composition::ElementKey{key});
  }

  static auto cells(std::size_t count) -> std::vector<composition::Element> {
    std::vector<composition::Element> made;
    for (std::size_t index = 0; index < count; ++index) {
      made.push_back(cell(std::format("c{}", index)));
    }
    return made;
  }

  static auto proposed(composition::Element grid, float width)
      -> composition::Element {
    return components::VStack(std::move(grid).id("g"))
        .frame({.width = width})
        .id("o");
  }

  void expect_tracks(std::vector<GridItem> columns, float width,
                     std::span<const Expected> expected,
                     std::optional<theme::Space> spacing = std::nullopt) {
    const auto layout = tree(
        proposed(components::LazyVGrid(
                     components::LazyVGridOptions{.columns = std::move(columns),
                                                  .spacing = spacing},
                     cells(expected.size())),
                 width));
    static_cast<void>(run(*layout));
    const composition::Rect grid = frame(*layout, "g");
    for (std::size_t index = 0; index < expected.size(); ++index) {
      const composition::Rect found = frame(*layout, std::format("c{}", index));
      EXPECT_NEAR(found.x - grid.x, expected[index].x, tolerance) << index;
      EXPECT_NEAR(found.width, expected[index].width, tolerance) << index;
      EXPECT_NEAR(found.y - grid.y, expected[index].y, tolerance) << index;
    }
  }

  static auto at(const composition::Tree& tree, std::string_view key,
                 const composition::Rect& origin) -> composition::Rect {
    const composition::Rect found = frame(tree, key);
    return composition::Rect{.x = found.x - origin.x,
                             .y = found.y - origin.y,
                             .width = found.width,
                             .height = found.height};
  }

  static void expect_rect(const composition::Rect& found, float x, float y,
                          float width, float height) {
    EXPECT_NEAR(found.x, x, tolerance);
    EXPECT_NEAR(found.y, y, tolerance);
    EXPECT_NEAR(found.width, width, tolerance);
    EXPECT_NEAR(found.height, height, tolerance);
  }
};

TEST_F(GridLayoutTest, FlexibleColumnsShareTheWidthLikeSwiftUI) {
  constexpr std::array expected{
      Expected{0.0F, 94.667F, 0.0F}, Expected{102.667F, 94.667F, 0.0F},
      Expected{205.333F, 94.667F, 0.0F}, Expected{0.0F, 94.667F, 28.0F}};
  expect_tracks(GridItem::repeated(3), 300.0F, expected);
}

TEST_F(GridLayoutTest, AdaptiveColumnsFitAsManyAsTheWidthAllows) {
  const std::vector adaptive{GridItem{GridItem::Size::adaptive(80.0F)}};
  constexpr std::array at300{
      Expected{0.0F, 94.667F, 0.0F}, Expected{102.667F, 94.667F, 0.0F},
      Expected{205.333F, 94.667F, 0.0F}, Expected{0.0F, 94.667F, 28.0F}};
  expect_tracks(adaptive, 300.0F, at300);
  constexpr std::array at350{
      Expected{0.0F, 81.5F, 0.0F}, Expected{89.5F, 81.5F, 0.0F},
      Expected{179.0F, 81.5F, 0.0F}, Expected{268.5F, 81.5F, 0.0F},
      Expected{0.0F, 81.5F, 28.0F}};
  expect_tracks(adaptive, 350.0F, at350);
  constexpr std::array at170{Expected{0.0F, 81.0F, 0.0F},
                             Expected{89.0F, 81.0F, 0.0F},
                             Expected{0.0F, 81.0F, 28.0F}};
  expect_tracks(adaptive, 170.0F, at170);
  constexpr std::array at60{Expected{0.0F, 60.0F, 0.0F},
                            Expected{0.0F, 60.0F, 28.0F}};
  expect_tracks(adaptive, 60.0F, at60);
}

TEST_F(GridLayoutTest, AdaptiveColumnsStopAtTheirMaximumAndCentre) {
  constexpr std::array expected{
      Expected{8.0F, 90.0F, 0.0F}, Expected{106.0F, 90.0F, 0.0F},
      Expected{204.0F, 90.0F, 0.0F}, Expected{302.0F, 90.0F, 0.0F},
      Expected{8.0F, 90.0F, 28.0F}};
  expect_tracks({GridItem{GridItem::Size::adaptive(80.0F, 90.0F)}}, 400.0F,
                expected);
  constexpr std::array spaced{
      Expected{0.0F, 100.0F, 0.0F}, Expected{100.0F, 100.0F, 0.0F},
      Expected{200.0F, 100.0F, 0.0F}, Expected{0.0F, 100.0F, 28.0F}};
  expect_tracks({GridItem{GridItem::Size::adaptive(80.0F), 0.0F}}, 300.0F,
                spaced);
}

TEST_F(GridLayoutTest, FixedAndFlexibleColumnsResolveInOrder) {
  constexpr std::array mixed{Expected{0.0F, 50.0F, 0.0F},
                             Expected{58.0F, 164.0F, 0.0F},
                             Expected{230.0F, 70.0F, 0.0F}};
  expect_tracks({GridItem{GridItem::Size::fixed(50.0F)}, GridItem{},
                 GridItem{GridItem::Size::fixed(70.0F)}},
                300.0F, mixed);
  constexpr std::array capped{Expected{43.0F, 146.0F, 0.0F},
                              Expected{197.0F, 60.0F, 0.0F}};
  expect_tracks({GridItem{GridItem::Size::flexible(100.0F)},
                 GridItem{GridItem::Size::flexible(10.0F, 60.0F)}},
                300.0F, capped);
  constexpr std::array first_capped{Expected{0.0F, 60.0F, 0.0F},
                                    Expected{68.0F, 232.0F, 0.0F}};
  expect_tracks({GridItem{GridItem::Size::flexible(10.0F, 60.0F)},
                 GridItem{GridItem::Size::flexible(100.0F)}},
                300.0F, first_capped);
  constexpr std::array fixed{Expected{96.0F, 50.0F, 0.0F},
                             Expected{154.0F, 50.0F, 0.0F}};
  expect_tracks({GridItem{GridItem::Size::fixed(50.0F)},
                 GridItem{GridItem::Size::fixed(50.0F)}},
                300.0F, fixed);
}

TEST_F(GridLayoutTest, AdaptiveItemsShareTheRoomLeftByTheirNeighbours) {
  constexpr std::array beside_fixed{
      Expected{0.0F, 58.667F, 0.0F}, Expected{66.667F, 58.667F, 0.0F},
      Expected{133.333F, 58.667F, 0.0F}, Expected{200.0F, 100.0F, 0.0F}};
  expect_tracks({GridItem{GridItem::Size::adaptive(50.0F)},
                 GridItem{GridItem::Size::fixed(100.0F)}},
                300.0F, beside_fixed);
  constexpr std::array beside_flexible{
      Expected{0.0F, 146.0F, 0.0F}, Expected{154.0F, 69.0F, 0.0F},
      Expected{231.0F, 69.0F, 0.0F}, Expected{0.0F, 146.0F, 28.0F}};
  expect_tracks({GridItem{}, GridItem{GridItem::Size::adaptive(50.0F)}}, 300.0F,
                beside_flexible);
}

TEST_F(GridLayoutTest, ItemSpacingFollowsEachItemAndRowSpacingTheGrid) {
  constexpr std::array spaced{Expected{0.0F, 93.333F, 0.0F},
                              Expected{113.333F, 93.333F, 0.0F},
                              Expected{206.667F, 93.333F, 0.0F}};
  expect_tracks({GridItem{GridItem::Size::flexible(), 20.0F},
                 GridItem{GridItem::Size::flexible(), 0.0F}, GridItem{}},
                300.0F, spaced);
  constexpr std::array rows{
      Expected{0.0F, 146.0F, 0.0F}, Expected{154.0F, 146.0F, 0.0F},
      Expected{0.0F, 146.0F, 32.0F}, Expected{154.0F, 146.0F, 32.0F}};
  expect_tracks(GridItem::repeated(2), 300.0F, rows, theme::Space{12.0F});
}

TEST_F(GridLayoutTest, OverflowingColumnsWidenTheGridAndCentreIt) {
  const auto layout = tree(
      proposed(components::LazyVGrid(
                   components::LazyVGridOptions{
                       .columns = {GridItem{GridItem::Size::flexible(200.0F)},
                                   GridItem{GridItem::Size::flexible(200.0F)}}},
                   cells(2)),
               300.0F));
  static_cast<void>(run(*layout));
  const composition::Rect outer = frame(*layout, "o");
  const composition::Rect grid = frame(*layout, "g");
  EXPECT_NEAR(grid.width, 408.0F, tolerance);
  EXPECT_NEAR(grid.x - outer.x, -54.0F, tolerance);
  EXPECT_NEAR(frame(*layout, "c1").x - grid.x, 208.0F, tolerance);
}

TEST_F(GridLayoutTest, CellsSitAtTheItemOrGridAlignment) {
  std::vector<composition::Element> boxes;
  boxes.push_back(box("a0", 20.0F, 20.0F));
  boxes.push_back(box("a1", 20.0F, 20.0F));
  boxes.push_back(box("a2", 20.0F, 20.0F));
  boxes.push_back(box("a3", 20.0F, 60.0F));
  const auto layout = tree(proposed(
      components::LazyVGrid(
          components::LazyVGridOptions{
              .columns = {GridItem{},
                          GridItem{GridItem::Size::flexible(), std::nullopt,
                                   composition::Alignment::leading},
                          GridItem{GridItem::Size::flexible(), std::nullopt,
                                   composition::Alignment::bottom_trailing}}},
          std::move(boxes)),
      300.0F));
  static_cast<void>(run(*layout));
  const composition::Rect grid = frame(*layout, "g");
  EXPECT_NEAR(at(*layout, "a0", grid).x, 37.333F, tolerance);
  EXPECT_NEAR(at(*layout, "a1", grid).x, 102.667F, tolerance);
  EXPECT_NEAR(at(*layout, "a2", grid).x, 280.0F, tolerance);
  EXPECT_NEAR(at(*layout, "a2", grid).y, 0.0F, tolerance);
  EXPECT_NEAR(at(*layout, "a3", grid).x, 37.333F, tolerance);
  EXPECT_NEAR(at(*layout, "a3", grid).y, 28.0F, tolerance);

  std::vector<composition::Element> rows;
  rows.push_back(box("b0", 20.0F, 20.0F));
  rows.push_back(box("b1", 20.0F, 50.0F));
  rows.push_back(box("b2", 20.0F, 20.0F));
  const auto leading = tree(
      proposed(components::LazyVGrid(
                   components::LazyVGridOptions{
                       .columns = GridItem::repeated(2),
                       .alignment = components::HorizontalAlignment::leading},
                   std::move(rows)),
               300.0F));
  static_cast<void>(run(*leading));
  const composition::Rect origin = frame(*leading, "g");
  expect_rect(at(*leading, "b0", origin), 0.0F, 15.0F, 20.0F, 20.0F);
  expect_rect(at(*leading, "b1", origin), 154.0F, 0.0F, 20.0F, 50.0F);
  expect_rect(at(*leading, "b2", origin), 0.0F, 58.0F, 20.0F, 20.0F);
}

TEST_F(GridLayoutTest, HorizontalGridsFlowDownTheirRowsThenAcross) {
  std::vector<composition::Element> columns;
  for (int index = 0; index < 5; ++index) {
    columns.push_back(
        components::VStack()
            .frame({.width = 20.0F, .max_height = infinity})
            .id(composition::ElementKey{std::format("h{}", index)}));
  }
  const auto layout =
      tree(components::LazyHGrid(
               components::LazyHGridOptions{
                   .rows = {GridItem{GridItem::Size::fixed(30.0F)},
                            GridItem{GridItem::Size::fixed(40.0F), 4.0F},
                            GridItem{GridItem::Size::fixed(30.0F)}},
                   .spacing = 6.0F},
               std::move(columns))
               .frame({.height = 200.0F})
               .id("g"));
  static_cast<void>(run(*layout));
  const composition::Rect grid = frame(*layout, "g");
  EXPECT_NEAR(grid.width, 46.0F, tolerance);
  EXPECT_NEAR(grid.height, 200.0F, tolerance);
  expect_rect(at(*layout, "h0", grid), 0.0F, 44.0F, 20.0F, 30.0F);
  expect_rect(at(*layout, "h1", grid), 0.0F, 82.0F, 20.0F, 40.0F);
  expect_rect(at(*layout, "h2", grid), 0.0F, 126.0F, 20.0F, 30.0F);
  expect_rect(at(*layout, "h3", grid), 26.0F, 44.0F, 20.0F, 30.0F);
  expect_rect(at(*layout, "h4", grid), 26.0F, 82.0F, 20.0F, 40.0F);
}

TEST_F(GridLayoutTest, SectionHeadersSpanTheGridAndStartNewRows) {
  const std::vector<int> first{0, 1, 2, 3};
  const std::vector<int> second{0, 1};
  const auto layout = tree(proposed(
      components::LazyVGrid(
          components::LazyVGridOptions{.columns = GridItem::repeated(3)},
          components::GridSection(
              cell("s0h", 10.0F),
              components::ForEach(
                  first, [](int index) { return std::format("s0c{}", index); },
                  [](int index) { return cell(std::format("s0c{}", index)); })),
          components::GridSection(
              cell("s1h", 10.0F),
              components::ForEach(
                  second, [](int index) { return std::format("s1c{}", index); },
                  [](int index) {
                    return cell(std::format("s1c{}", index));
                  }))),
      300.0F));
  static_cast<void>(run(*layout));
  static_cast<void>(run(*layout));
  const composition::Rect grid = frame(*layout, "g");
  expect_rect(at(*layout, "s0h", grid), 0.0F, 0.0F, 300.0F, 10.0F);
  expect_rect(at(*layout, "s0c0", grid), 0.0F, 18.0F, 94.667F, 20.0F);
  expect_rect(at(*layout, "s0c3", grid), 0.0F, 46.0F, 94.667F, 20.0F);
  expect_rect(at(*layout, "s1h", grid), 0.0F, 74.0F, 300.0F, 10.0F);
  expect_rect(at(*layout, "s1c1", grid), 102.667F, 92.0F, 94.667F, 20.0F);
}

TEST_F(GridLayoutTest, GridColumnsTakeTheirWidestCellAndCentreByDefault) {
  const auto layout =
      tree(components::Grid(components::GridRow(box("r0c0", 30.0F, 20.0F),
                                                box("r0c1", 80.0F, 20.0F)),
                            components::GridRow(box("r1c0", 60.0F, 30.0F),
                                                box("r1c1", 20.0F, 20.0F)),
                            components::GridRow(box("r2c0", 10.0F, 20.0F)))
               .id("g"));
  static_cast<void>(run(*layout));
  const composition::Rect grid = frame(*layout, "g");
  EXPECT_NEAR(grid.width, 148.0F, tolerance);
  EXPECT_NEAR(grid.height, 86.0F, tolerance);
  expect_rect(at(*layout, "r0c0", grid), 15.0F, 0.0F, 30.0F, 20.0F);
  expect_rect(at(*layout, "r0c1", grid), 68.0F, 0.0F, 80.0F, 20.0F);
  expect_rect(at(*layout, "r1c0", grid), 0.0F, 28.0F, 60.0F, 30.0F);
  expect_rect(at(*layout, "r1c1", grid), 98.0F, 33.0F, 20.0F, 20.0F);
  expect_rect(at(*layout, "r2c0", grid), 25.0F, 66.0F, 10.0F, 20.0F);
}

TEST_F(GridLayoutTest, GridSpacingAndAlignmentComeFromItsOptions) {
  const auto layout =
      tree(components::Grid(
               components::GridOptions{
                   .alignment = composition::Alignment::top_leading,
                   .horizontal_spacing = 4.0F,
                   .vertical_spacing = 6.0F},
               components::GridRow(box("r0c0", 30.0F, 20.0F),
                                   box("r0c1", 80.0F, 20.0F)),
               components::GridRow(box("r1c0", 60.0F, 30.0F),
                                   box("r1c1", 20.0F, 20.0F)))
               .id("g"));
  static_cast<void>(run(*layout));
  const composition::Rect grid = frame(*layout, "g");
  EXPECT_NEAR(grid.width, 144.0F, tolerance);
  EXPECT_NEAR(grid.height, 56.0F, tolerance);
  expect_rect(at(*layout, "r0c0", grid), 0.0F, 0.0F, 30.0F, 20.0F);
  expect_rect(at(*layout, "r0c1", grid), 64.0F, 0.0F, 80.0F, 20.0F);
  expect_rect(at(*layout, "r1c0", grid), 0.0F, 26.0F, 60.0F, 30.0F);
  expect_rect(at(*layout, "r1c1", grid), 64.0F, 26.0F, 20.0F, 20.0F);
}

TEST_F(GridLayoutTest, ColumnAndRowAlignmentPlaceEveryCellInTheirLine) {
  const auto columns = tree(
      components::Grid(components::GridRow(
                           box("r0c0", 30.0F, 20.0F)
                               .grid_column_alignment(
                                   composition::HorizontalAlignment::leading),
                           box("r0c1", 80.0F, 20.0F)
                               .grid_column_alignment(
                                   composition::HorizontalAlignment::trailing)),
                       components::GridRow(box("r1c0", 60.0F, 20.0F),
                                           box("r1c1", 20.0F, 20.0F)))
          .id("g"));
  static_cast<void>(run(*columns));
  const composition::Rect grid = frame(*columns, "g");
  expect_rect(at(*columns, "r0c0", grid), 0.0F, 0.0F, 30.0F, 20.0F);
  expect_rect(at(*columns, "r1c1", grid), 128.0F, 28.0F, 20.0F, 20.0F);

  const auto rows =
      tree(components::Grid(
               components::GridRow(
                   components::GridRowOptions{
                       .alignment = composition::VerticalAlignment::top},
                   box("r0c0", 30.0F, 50.0F), box("r0c1", 30.0F, 20.0F)),
               components::GridRow(
                   components::GridRowOptions{
                       .alignment = composition::VerticalAlignment::bottom},
                   box("r1c0", 30.0F, 20.0F), box("r1c1", 30.0F, 50.0F)))
               .id("g"));
  static_cast<void>(run(*rows));
  const composition::Rect table = frame(*rows, "g");
  EXPECT_NEAR(table.height, 108.0F, tolerance);
  expect_rect(at(*rows, "r0c1", table), 38.0F, 0.0F, 30.0F, 20.0F);
  expect_rect(at(*rows, "r1c0", table), 0.0F, 88.0F, 30.0F, 20.0F);
}

TEST_F(GridLayoutTest, SpanningCellsWidenTheColumnsTheyCover) {
  const auto layout = tree(
      components::Grid(
          components::GridRow(box("r0c0", 30.0F, 20.0F),
                              box("r0c1", 30.0F, 20.0F),
                              box("r0c2", 30.0F, 20.0F)),
          components::GridRow(box("r1span", 50.0F, 20.0F).grid_cell_columns(2),
                              box("r1c2", 30.0F, 20.0F)),
          components::GridRow(box("r2wide", 150.0F, 20.0F).grid_cell_columns(2),
                              box("r2c2", 30.0F, 20.0F)))
          .id("g"));
  static_cast<void>(run(*layout));
  const composition::Rect grid = frame(*layout, "g");
  EXPECT_NEAR(grid.width, 188.0F, tolerance);
  EXPECT_NEAR(at(*layout, "r0c0", grid).x, 20.5F, tolerance);
  EXPECT_NEAR(at(*layout, "r0c1", grid).x, 99.5F, tolerance);
  EXPECT_NEAR(at(*layout, "r0c2", grid).x, 158.0F, tolerance);
  EXPECT_NEAR(at(*layout, "r1span", grid).x, 50.0F, tolerance);
  EXPECT_NEAR(at(*layout, "r2wide", grid).x, 0.0F, tolerance);
  EXPECT_NEAR(at(*layout, "r2c2", grid).y, 56.0F, tolerance);
}

TEST_F(GridLayoutTest, CellAnchorsPlaceACellInsideItsArea) {
  const auto layout = tree(
      components::Grid(
          components::GridRow(box("r0c0", 80.0F, 60.0F),
                              box("r0c1", 80.0F, 60.0F)),
          components::GridRow(
              box("r1c0", 20.0F, 20.0F)
                  .grid_cell_anchor(motion::UnitPoint::top_leading),
              box("r1c1", 20.0F, 20.0F)
                  .grid_cell_anchor(motion::UnitPoint{.x = 0.75F, .y = 1.0F})))
          .id("g"));
  static_cast<void>(run(*layout));
  const composition::Rect grid = frame(*layout, "g");
  EXPECT_NEAR(grid.width, 168.0F, tolerance);
  EXPECT_NEAR(grid.height, 88.0F, tolerance);
  expect_rect(at(*layout, "r1c0", grid), 0.0F, 68.0F, 20.0F, 20.0F);
  expect_rect(at(*layout, "r1c1", grid), 133.0F, 68.0F, 20.0F, 20.0F);
}

TEST_F(GridLayoutTest, CellsOutsideRowsSpanTheWholeGrid) {
  const auto layout = tree(
      components::Grid(components::GridRow(box("r0c0", 30.0F, 20.0F),
                                           box("r0c1", 40.0F, 20.0F)),
                       components::Divider().id("divider"), cell("bar", 4.0F),
                       components::GridRow(box("r1c0", 30.0F, 20.0F),
                                           box("r1c1", 40.0F, 20.0F)))
          .id("g"));
  static_cast<void>(run(*layout));
  const composition::Rect grid = frame(*layout, "g");
  EXPECT_NEAR(grid.height, 69.0F, tolerance);
  expect_rect(at(*layout, "divider", grid), 0.0F, 28.0F, grid.width, 1.0F);
  expect_rect(at(*layout, "bar", grid), 0.0F, 37.0F, grid.width, 4.0F);
  EXPECT_NEAR(at(*layout, "r1c0", grid).y, 49.0F, tolerance);
  EXPECT_NEAR(at(*layout, "r1c1", grid).y, 49.0F, tolerance);
}

TEST_F(GridLayoutTest, FlexibleCellsWidenTheirColumnsAndUnsizedOnesDoNot) {
  const auto flexible = tree(
      proposed(components::Grid(
                   components::GridRow(box("r0c0", 30.0F, 20.0F), cell("r0c1"),
                                       box("r0c2", 40.0F, 20.0F)),
                   components::GridRow(box("r1c0", 50.0F, 20.0F),
                                       box("r1c1", 20.0F, 20.0F),
                                       box("r1c2", 20.0F, 20.0F))),
               300.0F));
  static_cast<void>(run(*flexible));
  const composition::Rect grid = frame(*flexible, "g");
  EXPECT_NEAR(grid.width, 300.0F, tolerance);
  expect_rect(at(*flexible, "r0c0", grid), 10.0F, 0.0F, 30.0F, 20.0F);
  expect_rect(at(*flexible, "r0c1", grid), 58.0F, 0.0F, 194.0F, 20.0F);
  expect_rect(at(*flexible, "r0c2", grid), 260.0F, 0.0F, 40.0F, 20.0F);
  expect_rect(at(*flexible, "r1c1", grid), 145.0F, 28.0F, 20.0F, 20.0F);

  const auto wide = tree(
      proposed(components::Grid(components::GridRow(box("r0c0", 30.0F, 20.0F),
                                                    box("r0c1", 40.0F, 20.0F)),
                                components::Divider().id("divider"),
                                components::GridRow(box("r1c0", 30.0F, 20.0F),
                                                    box("r1c1", 40.0F, 20.0F))),
               300.0F));
  static_cast<void>(run(*wide));
  const composition::Rect spread = frame(*wide, "g");
  EXPECT_NEAR(spread.width, 300.0F, tolerance);
  EXPECT_NEAR(at(*wide, "r0c0", spread).x, 55.5F, tolerance);
  EXPECT_NEAR(at(*wide, "r0c1", spread).x, 204.5F, tolerance);
  EXPECT_NEAR(at(*wide, "divider", spread).width, 300.0F, tolerance);

  const auto unsized = tree(proposed(
      components::Grid(
          components::GridRow(box("r0c0", 30.0F, 20.0F),
                              box("r0c1", 40.0F, 20.0F)),
          cell("bar", 4.0F)
              .grid_cell_unsized_axes(composition::Axes::horizontal),
          components::GridRow(
              components::VStack()
                  .frame({.max_width = infinity, .max_height = infinity})
                  .grid_cell_unsized_axes(composition::Axes::all)
                  .id("fill"),
              box("r1c1", 40.0F, 20.0F))),
      300.0F));
  static_cast<void>(run(*unsized));
  const composition::Rect kept = frame(*unsized, "g");
  EXPECT_NEAR(kept.width, 78.0F, tolerance);
  expect_rect(at(*unsized, "bar", kept), 0.0F, 28.0F, 78.0F, 4.0F);
  expect_rect(at(*unsized, "fill", kept), 0.0F, 40.0F, 30.0F, 20.0F);
}

TEST_F(GridLayoutTest, TracksAreResolvedWithoutAllocatingToCountThem) {
  const theme::Theme& theme = components::Environment::theme.fallback();
  const std::vector adaptive{GridItem{GridItem::Size::adaptive(80.0F)}};
  EXPECT_EQ(components::GridTracks::count(adaptive, 350.0F, theme), 4U);
  EXPECT_EQ(components::GridTracks::count(adaptive, 60.0F, theme), 1U);
  const components::GridTracks tracks(
      adaptive, components::GridTracks::spacings(adaptive, theme), 350.0F);
  EXPECT_EQ(tracks.count(), 4U);
  EXPECT_NEAR(tracks.extent(), 350.0F, tolerance);
  EXPECT_NEAR(tracks.tracks()[3].offset, 268.5F, tolerance);
}

TEST_F(GridLayoutTest, RowsOfTextSitTogetherAndOtherRowsTakeTheStackSpacing) {
  const auto layout = tree(
      components::Grid(components::GridRow(components::Text("Name").id("t0"),
                                           components::Text("Value").id("t1")),
                       components::GridRow(components::Text("Mode").id("t2"),
                                           components::Text("Docked").id("t3")),
                       components::GridRow(box("b0", 20.0F, 20.0F)))
          .id("g"));
  static_cast<void>(run(*layout));
  const composition::Rect first = frame(*layout, "t0");
  const composition::Rect second = frame(*layout, "t2");
  const composition::Rect boxed = frame(*layout, "b0");
  EXPECT_NEAR(second.y - (first.y + first.height), 0.0F, tolerance);
  EXPECT_NEAR(boxed.y - (second.y + second.height), 8.0F, tolerance);
}

TEST_F(GridLayoutTest, AGridHugsItsColumnsAndTheFrameAlignsItLikeSwiftUI) {
  struct Case {
    composition::Alignment alignment;
    components::LayoutDirection direction;
    float a_x;
    float b_x;
  };
  for (const Case& each :
       {Case{composition::Alignment::leading,
             components::LayoutDirection::left_to_right, 0.0F, 50.0F},
        Case{composition::Alignment::center,
             components::LayoutDirection::left_to_right, 95.0F, 145.0F},
        Case{composition::Alignment::trailing,
             components::LayoutDirection::left_to_right, 190.0F, 240.0F},
        Case{composition::Alignment::leading,
             components::LayoutDirection::right_to_left, 260.0F, 190.0F}}) {
    const auto layout = tree(
        components::Grid(
            components::GridOptions{.horizontal_spacing = 10.0F},
            components::GridRow(box("a", 40.0F, 10.0F), box("b", 60.0F, 10.0F)))
            .frame(
                {.width = 300.0F, .height = 60.0F, .alignment = each.alignment})
            .environment(components::Environment::layout_direction,
                         each.direction)
            .id("frame"));
    static_cast<void>(run(*layout));
    const composition::Rect outer = frame(*layout, "frame");
    expect_rect(at(*layout, "a", outer), each.a_x, 25.0F, 40.0F, 10.0F);
    expect_rect(at(*layout, "b", outer), each.b_x, 25.0F, 60.0F, 10.0F);
  }
}

}
