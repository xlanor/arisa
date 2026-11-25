#include "focus_fixture.hpp"

#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/motion/unit_point.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/structure/callout_board.hpp>

#include <gtest/gtest.h>

#include <format>
#include <string>
#include <utility>
#include <vector>

namespace arisa::runtime {

class CalloutBoardTest : public FocusFixture {
 protected:
  using Board = structure::CalloutBoard;

  static auto chip(std::size_t index) -> std::string {
    return std::format("{}.{}", Board::chip_key, index);
  }

  static auto board() -> composition::Element {
    std::vector<Board::Callout> callouts;
    callouts.push_back(Board::Callout{.chip = components::Text("Cross"),
                                      .anchor = {.x = 0.8F, .y = 0.7F},
                                      .side = Board::Side::leading});
    callouts.push_back(Board::Callout{.chip = components::Text("L1"),
                                      .anchor = {.x = 0.2F, .y = 0.1F},
                                      .side = Board::Side::leading});
    callouts.push_back(Board::Callout{.chip = components::Text("Options"),
                                      .anchor = {.x = 0.7F, .y = 0.3F},
                                      .side = Board::Side::trailing});
    return Board(components::ZStack(), std::move(callouts),
                 Board::Options{.art = {.width = 200.0F, .height = 100.0F},
                                .column_width = 120.0F,
                                .gap = 30.0F});
  }

  provider::headless::HeadlessProviderFactory providers_;
};

TEST_F(CalloutBoardTest, LeadersRunFromEachChipColumnToItsAnchor) {
  const Board::Options options{.art = {.width = 200.0F, .height = 100.0F},
                               .column_width = 120.0F,
                               .gap = 30.0F};
  const std::vector<Board::Side> sides{
      Board::Side::leading, Board::Side::leading, Board::Side::trailing};
  const std::vector<Board::Leader> lines = Board::leaders(
      {{.x = 0.8F, .y = 0.7F}, {.x = 0.2F, .y = 0.1F}, {.x = 0.7F, .y = 0.3F}},
      sides, options);
  ASSERT_EQ(lines.size(), 3U);
  EXPECT_FLOAT_EQ(Board::height(sides, options), 132.0F);
  EXPECT_FLOAT_EQ(lines[1].from.y, 19.0F);
  EXPECT_FLOAT_EQ(lines[0].from.y, 113.0F);
  EXPECT_FLOAT_EQ(lines[0].from.x, 120.0F);
  EXPECT_FLOAT_EQ(lines[0].elbow.x, 136.0F);
  EXPECT_FLOAT_EQ(lines[0].elbow.y, 113.0F);
  EXPECT_FLOAT_EQ(lines[0].to.x, 150.0F + 160.0F);
  EXPECT_FLOAT_EQ(lines[0].to.y, 16.0F + 70.0F);
  EXPECT_FLOAT_EQ(lines[2].from.x, 120.0F + 30.0F + 200.0F + 30.0F);
  EXPECT_FLOAT_EQ(lines[2].elbow.x, 380.0F - 16.0F);
  EXPECT_FLOAT_EQ(lines[2].from.y, 66.0F);
  EXPECT_FLOAT_EQ(Board::width(options), 500.0F);
}

TEST_F(CalloutBoardTest, TallColumnsGrowTheBoardInsteadOfOverlapping) {
  const Board::Options options{.art = {.width = 200.0F, .height = 100.0F},
                               .column_width = 120.0F,
                               .gap = 30.0F};
  std::vector<motion::UnitPoint> anchors;
  std::vector<Board::Side> sides;
  for (int index = 0; index < 9; ++index) {
    anchors.push_back({.x = 0.2F, .y = static_cast<float>(index) / 10.0F});
    sides.push_back(Board::Side::leading);
  }
  const std::vector<Board::Leader> lines =
      Board::leaders(anchors, sides, options);
  EXPECT_FLOAT_EQ(Board::height(sides, options), (9.0F * 46.0F) - 8.0F + 16.0F);
  for (std::size_t index = 1; index < lines.size(); ++index) {
    EXPECT_GE(lines[index].from.y - lines[index - 1].from.y,
              options.chip_height + options.chip_spacing);
  }
}

TEST_F(CalloutBoardTest, ChipsSitBesideTheArtInAnchorOrder) {
  Application application = start(providers_, board());
  const provider::PixelPoint art = centre(application, Board::art_key);
  const provider::PixelPoint cross = centre(application, chip(0));
  const provider::PixelPoint shoulder = centre(application, chip(1));
  const provider::PixelPoint options = centre(application, chip(2));
  EXPECT_LT(cross.x, art.x);
  EXPECT_LT(shoulder.x, art.x);
  EXPECT_GT(options.x, art.x);
  EXPECT_LT(shoulder.y, cross.y);
  EXPECT_NEAR(cross.y - shoulder.y, 94.0F, 0.5F);
  EXPECT_TRUE(node_of(application, Board::lines_key).has_value());
}


TEST_F(CalloutBoardTest, ContentBelowStartsUnderTheWholeBoard) {
  const Board::Options options{.art = {.width = 200.0F, .height = 100.0F},
                               .column_width = 120.0F,
                               .gap = 30.0F};
  std::vector<Board::Callout> callouts;
  std::vector<Board::Side> sides;
  for (int index = 0; index < 8; ++index) {
    callouts.push_back(Board::Callout{
        .chip = components::Text(std::format("Chip {}", index)),
        .anchor = {.x = 0.2F, .y = static_cast<float>(index) / 10.0F},
        .side = Board::Side::leading});
    sides.push_back(Board::Side::leading);
  }
  Application application = start(
      providers_,
      components::VStack(
          components::VStackOptions{.alignment =
                                        components::HorizontalAlignment::leading,
                                    .spacing = theme::Space{0.0F}},
          Board(components::ZStack(), std::move(callouts), options),
          components::Text("Below").id("below")));
  const auto board_top = node_named(application, chip(0));
  const auto below = node_named(application, "below");
  ASSERT_TRUE(board_top.has_value());
  ASSERT_TRUE(below.has_value());
  const float board = Board::height(sides, options);
  EXPECT_GE(below->get().frame().y - (board_top->get().frame().y), board - 0.5F);
}
}
