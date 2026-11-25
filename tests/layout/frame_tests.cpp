#include "layout_fixture.hpp"

#include <arisa/components/hstack.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/alignment.hpp>
#include <arisa/composition/axes.hpp>
#include <arisa/composition/modifiers.hpp>

#include <gtest/gtest.h>

#include <array>
#include <functional>
#include <optional>
#include <string_view>
#include <utility>

namespace arisa::layout {

using components::HorizontalAlignment;
using composition::Alignment;
using composition::FrameSpec;

class FrameTest : public ::testing::Test, protected LayoutFixture {
 protected:
  static auto laid(composition::Element content)
      -> std::unique_ptr<composition::Tree> {
    auto built = tree(std::move(content));
    auto context = LayoutContext::create();
    EXPECT_TRUE(context.has_value());
    EXPECT_TRUE(
        context->update(*built, handheld, std::ref(typesetter())).has_value());
    return built;
  }

  static auto framed(FrameSpec spec) -> composition::Element {
    return components::VStack(box("child", 40.0F, 20.0F))
        .frame(spec)
        .id("frame");
  }

  static auto placement(FrameSpec spec) -> composition::Rect {
    const auto built = laid(framed(spec));
    const composition::Rect outer = frame(*built, "frame");
    const composition::Rect inner = frame(*built, "child");
    return composition::Rect{.x = inner.x - outer.x,
                             .y = inner.y - outer.y,
                             .width = inner.width,
                             .height = inner.height};
  }

  static auto content_frame(const composition::Tree& built,
                            std::string_view key)
      -> std::optional<composition::Rect> {
    const auto found = built.find(composition::ElementKey{key});
    EXPECT_TRUE(found.has_value());
    return built.node(*found)->get().content_frame();
  }
};

TEST_F(FrameTest, ALargerFrameCentresItsChildByDefault) {
  const composition::Rect placed =
      placement({.width = 200.0F, .height = 100.0F});
  EXPECT_FLOAT_EQ(placed.x, 80.0F);
  EXPECT_FLOAT_EQ(placed.y, 40.0F);
  EXPECT_FLOAT_EQ(placed.width, 40.0F);
  EXPECT_FLOAT_EQ(placed.height, 20.0F);
}

TEST_F(FrameTest, FrameAlignmentPlacesTheChildOnEveryEdge) {
  constexpr std::array cases{
      std::pair{Alignment::top_leading, std::array{0.0F, 0.0F}},
      std::pair{Alignment::top, std::array{80.0F, 0.0F}},
      std::pair{Alignment::top_trailing, std::array{160.0F, 0.0F}},
      std::pair{Alignment::leading, std::array{0.0F, 40.0F}},
      std::pair{Alignment::trailing, std::array{160.0F, 40.0F}},
      std::pair{Alignment::bottom_leading, std::array{0.0F, 80.0F}},
      std::pair{Alignment::bottom, std::array{80.0F, 80.0F}},
      std::pair{Alignment::bottom_trailing, std::array{160.0F, 80.0F}},
  };
  for (const auto& [alignment, expected] : cases) {
    const composition::Rect placed =
        placement({.width = 200.0F, .height = 100.0F, .alignment = alignment});
    EXPECT_FLOAT_EQ(placed.x, expected[0]);
    EXPECT_FLOAT_EQ(placed.y, expected[1]);
  }
}

TEST_F(FrameTest, AFillingFrameCentresContentThatDoesNotStretch) {
  const auto built = laid(
      components::VStack(
          components::VStackOptions{.alignment = HorizontalAlignment::leading},
          box("narrow", 40.0F, 20.0F), box("wide", 100.0F, 20.0F))
          .frame({.max_width = composition::infinity})
          .id("column"));
  EXPECT_FLOAT_EQ(frame(*built, "column").width, 1280.0F);
  EXPECT_FLOAT_EQ(frame(*built, "wide").x, 590.0F);
  EXPECT_FLOAT_EQ(frame(*built, "narrow").x, 590.0F);
}

TEST_F(FrameTest, LeadingFrameAlignmentKeepsAFillingColumnFlush) {
  const auto built = laid(
      components::VStack(
          components::VStackOptions{.alignment = HorizontalAlignment::leading},
          box("narrow", 40.0F, 20.0F), box("wide", 100.0F, 20.0F))
          .frame({.max_width = composition::infinity,
                  .alignment = Alignment::leading})
          .id("column"));
  EXPECT_FLOAT_EQ(frame(*built, "wide").x, 0.0F);
  EXPECT_FLOAT_EQ(frame(*built, "narrow").x, 0.0F);
}

TEST_F(FrameTest, ARowWithASpacerStillFillsItsFrame) {
  const auto built =
      laid(components::HStack(box("left", 50.0F, 50.0F), components::Spacer(),
                              box("right", 30.0F, 20.0F))
               .frame({.max_width = composition::infinity})
               .id("row"));
  EXPECT_FLOAT_EQ(frame(*built, "left").x, 0.0F);
  EXPECT_FLOAT_EQ(frame(*built, "right").x, 1250.0F);
}

TEST_F(FrameTest, TextInsideAWiderFrameIsPlacedByTheAlignment) {
  const auto measure = [](Alignment alignment) {
    const auto built = laid(
        components::Text("Label")
            .frame({.width = 300.0F, .height = 80.0F, .alignment = alignment})
            .id("text"));
    const std::optional<composition::Rect> inside =
        content_frame(*built, "text");
    EXPECT_TRUE(inside.has_value());
    return inside.value_or(composition::Rect{});
  };
  const composition::Rect centred = measure(Alignment::center);
  EXPECT_GT(centred.width, 0.0F);
  EXPECT_LT(centred.width, 300.0F);
  EXPECT_NEAR(centred.x, (300.0F - centred.width) * 0.5F, 0.01F);
  EXPECT_NEAR(centred.y, (80.0F - centred.height) * 0.5F, 0.01F);
  const composition::Rect leading = measure(Alignment::top_leading);
  EXPECT_FLOAT_EQ(leading.x, 0.0F);
  EXPECT_FLOAT_EQ(leading.y, 0.0F);
  const composition::Rect trailing = measure(Alignment::bottom_trailing);
  EXPECT_NEAR(trailing.x + trailing.width, 300.0F, 0.01F);
  EXPECT_NEAR(trailing.y + trailing.height, 80.0F, 0.01F);
}

TEST_F(FrameTest, AnUnframedTextHasNoAlignedContentFrame) {
  const auto built = laid(components::Text("Label").padding(4.0F).id("text"));
  EXPECT_FALSE(content_frame(*built, "text").has_value());
}

TEST_F(FrameTest, IdealWidthSizesAChildAlongAnOpenScrollAxis) {
  const auto built = laid(components::ScrollView(
      composition::Axes::horizontal,
      components::HStack(components::VStack(box("inside", 20.0F, 20.0F))
                             .frame({.ideal_width = 150.0F})
                             .id("ideal"))));
  EXPECT_FLOAT_EQ(frame(*built, "ideal").width, 150.0F);
  const auto outside = laid(components::VStack(box("inside", 20.0F, 20.0F))
                                .frame({.ideal_width = 150.0F})
                                .id("ideal"));
  EXPECT_FLOAT_EQ(frame(*outside, "ideal").width, 20.0F);
}

TEST_F(FrameTest, IdealHeightSizesAChildAlongAnOpenScrollAxis) {
  const auto built = laid(components::ScrollView(
      components::VStack(components::VStack(box("inside", 20.0F, 20.0F))
                             .frame({.ideal_height = 90.0F})
                             .id("ideal"))));
  EXPECT_FLOAT_EQ(frame(*built, "ideal").height, 90.0F);
}

TEST_F(FrameTest, EveryFrameFieldChangesLayout) {
  const auto measured = [](FrameSpec spec,
                           std::optional<composition::Axes> scrolling) {
    composition::Element content =
        components::ZStack(box("child", 40.0F, 20.0F)).frame(spec).id("frame");
    if (scrolling.has_value()) {
      content = components::ScrollView(
          *scrolling,
          components::HStack(components::VStack(std::move(content))));
    }
    const auto built = laid(std::move(content));
    const composition::Rect outer = frame(*built, "frame");
    const composition::Rect inner = frame(*built, "child");
    return std::array{outer.width, outer.height, inner.x - outer.x,
                      inner.y - outer.y};
  };
  const auto plain = measured({}, std::nullopt);
  EXPECT_NE(measured({.width = 90.0F}, std::nullopt), plain);
  EXPECT_NE(measured({.height = 90.0F}, std::nullopt), plain);
  EXPECT_NE(measured({.min_width = 90.0F}, std::nullopt), plain);
  EXPECT_NE(measured({.ideal_width = 90.0F}, composition::Axes::horizontal),
            measured({}, composition::Axes::horizontal));
  EXPECT_NE(measured({.max_width = 10.0F}, std::nullopt), plain);
  EXPECT_NE(measured({.min_height = 90.0F}, std::nullopt), plain);
  EXPECT_NE(measured({.ideal_height = 90.0F}, composition::Axes::vertical),
            measured({}, composition::Axes::vertical));
  EXPECT_NE(measured({.max_height = 10.0F}, std::nullopt), plain);
  EXPECT_NE(
      measured({.width = 90.0F, .alignment = Alignment::leading}, std::nullopt),
      measured({.width = 90.0F}, std::nullopt));
}

}

namespace arisa::layout {

TEST_F(FrameTest, ARowInsideAFillingFrameStillWrapsItsText) {
  const auto built = laid(
      components::VStack(
          components::HStack(
              components::Text("fixed").frame({.width = 160.0F}),
              components::Text("A long label that has to wrap inside the row "
                               "because the row is only as wide as the page")
                  .id("long"))
              .frame({.max_width = composition::infinity,
                      .alignment = Alignment::leading})
              .id("row"))
          .frame({.width = 400.0F}));
  const composition::Rect row = frame(*built, "row");
  const composition::Rect text = frame(*built, "long");
  EXPECT_FLOAT_EQ(row.width, 400.0F);
  EXPECT_LE(text.x + text.width, row.x + row.width + 0.01F);
}

}

namespace arisa::layout {

TEST_F(FrameTest, AFramedColumnInsideAScrollViewKeepsItsFullHeight) {
  const auto built = laid(
      components::ScrollView(components::VStack(box("first", 40.0F, 300.0F),
                                                box("second", 40.0F, 300.0F),
                                                box("last", 40.0F, 300.0F))
                                 .frame({.max_width = composition::infinity})
                                 .id("column"))
          .frame({.width = 200.0F, .height = 200.0F})
          .id("scroll"));
  EXPECT_GE(frame(*built, "column").height, 900.0F);
  const auto found = built->find(composition::ElementKey{"scroll"});
  ASSERT_TRUE(found.has_value());
  EXPECT_GE(built->node(*found)
                ->get()
                .content_size()
                .value_or(composition::Size{})
                .height,
            900.0F);
}

}

namespace arisa::layout {

TEST_F(FrameTest, AFlexibleFrameDirectlyInAScrollViewTakesItsIdealSize) {
  const auto built = laid(
      components::ScrollView(components::ZStack()
                                 .frame({.ideal_width = 10.0F,
                                         .max_width = composition::infinity,
                                         .ideal_height = 10.0F,
                                         .max_height = composition::infinity})
                                 .id("colour"))
          .frame({.width = 200.0F, .height = 300.0F}));
  EXPECT_FLOAT_EQ(frame(*built, "colour").width, 200.0F);
  EXPECT_FLOAT_EQ(frame(*built, "colour").height, 10.0F);
}

TEST_F(FrameTest, ANestedScrollPanelIsBoundedByTheOuterViewport) {
  const auto built = laid(
      components::ScrollView(
          components::HStack(
              components::HStackOptions{.alignment =
                                            components::VerticalAlignment::top},
              components::ScrollView(
                  components::VStack(box("first", 40.0F, 600.0F),
                                     box("last", 40.0F, 600.0F))
                      .frame({.max_width = composition::infinity}))
                  .frame({.width = 200.0F, .max_height = composition::infinity})
                  .id("panel"),
              components::VStack(box("preview", 40.0F, 40.0F))
                  .frame({.max_width = composition::infinity,
                          .max_height = composition::infinity}))
              .frame({.max_width = composition::infinity})
              .container_relative_frame(composition::Axes::vertical)
              .id("row"))
          .frame({.max_width = composition::infinity,
                  .max_height = composition::infinity}));
  EXPECT_FLOAT_EQ(frame(*built, "row").height, 720.0F);
  EXPECT_FLOAT_EQ(frame(*built, "panel").height, 720.0F);
}

}
