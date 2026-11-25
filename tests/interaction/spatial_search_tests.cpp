#include <arisa/composition/move_command_direction.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/interaction/spatial_search.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <optional>
#include <vector>

namespace arisa::interaction {

using composition::MoveCommandDirection;
using composition::Rect;

class SpatialSearchTest : public ::testing::Test {
 protected:
  static auto grid() -> std::vector<Rect> {
    std::vector<Rect> cells;
    for (int row = 0; row < 3; ++row) {
      for (int column = 0; column < 3; ++column) {
        cells.push_back(Rect{.x = static_cast<float>(column) * 110.0F,
                             .y = static_cast<float>(row) * 60.0F,
                             .width = 100.0F,
                             .height = 50.0F});
      }
    }
    return cells;
  }

  static auto without(std::vector<Rect> cells, std::size_t index)
      -> std::vector<Rect> {
    cells.erase(cells.begin() + static_cast<std::ptrdiff_t>(index));
    return cells;
  }
};

TEST_F(SpatialSearchTest, GridMovesToTheAdjacentCellInEachDirection) {
  const std::vector<Rect> cells = grid();
  const SpatialSearch search;
  const Rect centre = cells[4];
  EXPECT_EQ(search.nearest(centre, MoveCommandDirection::right, cells), 5U);
  EXPECT_EQ(search.nearest(centre, MoveCommandDirection::left, cells), 3U);
  EXPECT_EQ(search.nearest(centre, MoveCommandDirection::up, cells), 1U);
  EXPECT_EQ(search.nearest(centre, MoveCommandDirection::down, cells), 7U);
}

TEST_F(SpatialSearchTest, EdgesHaveNoCandidate) {
  const std::vector<Rect> cells = grid();
  const SpatialSearch search;
  EXPECT_FALSE(
      search.nearest(cells[0], MoveCommandDirection::left, cells).has_value());
  EXPECT_FALSE(
      search.nearest(cells[0], MoveCommandDirection::up, cells).has_value());
  EXPECT_FALSE(
      search.nearest(cells[8], MoveCommandDirection::right, cells).has_value());
  EXPECT_FALSE(
      search.nearest(cells[8], MoveCommandDirection::down, cells).has_value());
}

TEST_F(SpatialSearchTest, UnevenRowsPreferTheMostAlignedCell) {
  const std::vector<Rect> top{
      Rect{.x = 0.0F, .y = 0.0F, .width = 200.0F, .height = 40.0F},
      Rect{.x = 210.0F, .y = 0.0F, .width = 100.0F, .height = 40.0F},
      Rect{.x = 320.0F, .y = 0.0F, .width = 100.0F, .height = 40.0F}};
  const Rect wide{.x = 110.0F, .y = 50.0F, .width = 300.0F, .height = 40.0F};
  const SpatialSearch search;
  EXPECT_EQ(search.nearest(wide, MoveCommandDirection::up, top), 1U);
  const Rect narrow{.x = 0.0F, .y = 50.0F, .width = 60.0F, .height = 40.0F};
  EXPECT_EQ(search.nearest(narrow, MoveCommandDirection::up, top), 0U);
}

TEST_F(SpatialSearchTest, AlignedCandidatesBeatCloserDiagonalOnes) {
  const Rect from{.x = 0.0F, .y = 0.0F, .width = 100.0F, .height = 40.0F};
  const std::vector<Rect> candidates{
      Rect{.x = 120.0F, .y = 50.0F, .width = 100.0F, .height = 40.0F},
      Rect{.x = 600.0F, .y = 0.0F, .width = 100.0F, .height = 40.0F}};
  EXPECT_EQ(
      SpatialSearch{}.nearest(from, MoveCommandDirection::right, candidates),
      1U);
}

TEST_F(SpatialSearchTest, AlignmentBiasWeighsTheCrossAxis) {
  const Rect from{.x = 0.0F, .y = 100.0F, .width = 50.0F, .height = 50.0F};
  const std::vector<Rect> candidates{
      Rect{.x = 60.0F, .y = 0.0F, .width = 50.0F, .height = 50.0F},
      Rect{.x = 300.0F, .y = 155.0F, .width = 50.0F, .height = 50.0F}};
  EXPECT_EQ(SpatialSearch{0.0F}.nearest(from, MoveCommandDirection::right,
                                        candidates),
            0U);
  EXPECT_EQ(SpatialSearch{10.0F}.nearest(from, MoveCommandDirection::right,
                                         candidates),
            1U);
}

TEST_F(SpatialSearchTest, NearerOverlappingRowBeatsACentredFartherOne) {
  const Rect segmented{.x = 0.0F, .y = 0.0F, .width = 300.0F, .height = 32.0F};
  const std::vector<Rect> candidates{
      Rect{.x = 0.0F, .y = 96.0F, .width = 300.0F, .height = 40.0F},
      Rect{.x = 170.0F, .y = 44.0F, .width = 130.0F, .height = 36.0F}};
  EXPECT_EQ(SpatialSearch{}.nearest(segmented, MoveCommandDirection::down,
                                    candidates),
            1U);
}

TEST_F(SpatialSearchTest, ClosestIgnoresDirection) {
  const std::vector<Rect> cells = grid();
  const Rect probe{.x = 230.0F, .y = 130.0F, .width = 10.0F, .height = 10.0F};
  EXPECT_EQ(SpatialSearch::closest(probe, cells), 8U);
  EXPECT_FALSE(SpatialSearch::closest(probe, {}).has_value());
}

TEST_F(SpatialSearchTest, OverlappingFramesStillProgress) {
  const Rect from{.x = 0.0F, .y = 0.0F, .width = 100.0F, .height = 50.0F};
  const std::vector<Rect> candidates{
      Rect{.x = 80.0F, .y = 0.0F, .width = 100.0F, .height = 50.0F}};
  EXPECT_EQ(
      SpatialSearch{}.nearest(from, MoveCommandDirection::right, candidates),
      0U);
  EXPECT_FALSE(SpatialSearch{}
                   .nearest(from, MoveCommandDirection::left, candidates)
                   .has_value());
}

}
