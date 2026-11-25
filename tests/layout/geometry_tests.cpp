#include "layout_fixture.hpp"

#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/divider.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/for_each.hpp>
#include <arisa/components/geometry_reader.hpp>
#include <arisa/components/grid.hpp>
#include <arisa/components/grid_row.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/alignment.hpp>
#include <arisa/composition/axes.hpp>
#include <arisa/composition/axis.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/coordinate_space.hpp>
#include <arisa/composition/geometry_effect.hpp>
#include <arisa/composition/geometry_proxy.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/modifiers.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/composition/reserved_region.hpp>
#include <arisa/composition/safe_area.hpp>
#include <arisa/composition/safe_area_regions.hpp>
#include <arisa/composition/scene_padding.hpp>
#include <arisa/graphics/affine_transform.hpp>
#include <arisa/graphics/projection_transform.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::layout {

using composition::Alignment;
using composition::Axes;
using composition::infinity;

class GeometryTest : public ::testing::Test, protected LayoutFixture {
 protected:
  static constexpr std::string_view paragraph =
      "A line of text long enough to wrap inside two hundred points";
  static constexpr std::string_view title = "A long title that truncates";
  static constexpr float tolerance = 0.01F;

  struct Laid {
    std::unique_ptr<composition::Tree> tree;
    LayoutContext context;
  };

  static auto laid(composition::Element content) -> Laid {
    auto built = tree(std::move(content));
    auto context = LayoutContext::create();
    EXPECT_TRUE(context.has_value());
    EXPECT_TRUE(
        context->update(*built, handheld, std::ref(typesetter())).has_value());
    for (int round = 0; round < 2 && built->dirty(); ++round) {
      built->reconcile();
      EXPECT_TRUE(context->update(*built, handheld, std::ref(typesetter()))
                      .has_value());
    }
    return Laid{.tree = std::move(built), .context = std::move(*context)};
  }

  static auto text(std::string_view content, std::string_view key)
      -> composition::Element {
    return components::Text(std::string{content})
        .line_limit(1)
        .id(composition::ElementKey{key});
  }

  static auto offset(const composition::Tree& built, std::string_view inner,
                     std::string_view outer) -> composition::Rect {
    const composition::Rect in = frame(built, inner);
    const composition::Rect out = frame(built, outer);
    return composition::Rect{.x = in.x - out.x,
                             .y = in.y - out.y,
                             .width = in.width,
                             .height = in.height};
  }

  static auto natural(std::string_view content) -> composition::Rect {
    const Laid probe =
        laid(components::HStack(components::Text(std::string{content}).id("n"))
                 .fixed_size());
    return frame(*probe.tree, "n");
  }
};

TEST_F(GeometryTest, FixedSizeGivesTextItsIdealWidthInsideANarrowFrame) {
  const composition::Rect ideal = natural(paragraph);
  const Laid result =
      laid(components::VStack(
               components::Text(std::string{paragraph}).fixed_size().id("t"))
               .frame({.width = 100.0F, .height = 60.0F}));
  const composition::Rect placed = frame(*result.tree, "t");
  EXPECT_GT(placed.width, 100.0F);
  EXPECT_NEAR(placed.width, ideal.width, tolerance);
  EXPECT_NEAR(placed.height, ideal.height, tolerance);
}

TEST_F(GeometryTest, VerticalFixedSizeWrapsInsideTheProposedWidth) {
  const composition::Rect ideal = natural(paragraph);
  const Laid result =
      laid(components::VStack(components::Text(std::string{paragraph})
                                  .fixed_size(false, true)
                                  .id("t"))
               .frame({.width = 100.0F, .height = 20.0F}));
  const composition::Rect placed = frame(*result.tree, "t");
  EXPECT_LE(placed.width, 100.0F + tolerance);
  EXPECT_GT(placed.height, ideal.height * 2.0F);
}

TEST_F(GeometryTest, FixedSizeUsesTheIdealFrame) {
  const Laid result =
      laid(components::VStack(components::VStack()
                                  .frame({.ideal_width = 120.0F,
                                          .max_width = infinity,
                                          .ideal_height = 30.0F,
                                          .max_height = infinity})
                                  .fixed_size()
                                  .id("box"))
               .frame({.width = 300.0F, .height = 100.0F}));
  const composition::Rect placed = frame(*result.tree, "box");
  EXPECT_NEAR(placed.width, 120.0F, tolerance);
  EXPECT_NEAR(placed.height, 30.0F, tolerance);
}

TEST_F(GeometryTest, AFixedRowIsTheSumOfItsLabels) {
  const Laid result =
      laid(components::HStack(components::HStackOptions{.spacing = 0.0F},
                              components::Text("Short").id("short"),
                              components::Text("A longer label").id("long"))
               .fixed_size()
               .id("row"));
  const composition::Rect row = frame(*result.tree, "row");
  EXPECT_NEAR(
      row.width,
      frame(*result.tree, "short").width + frame(*result.tree, "long").width,
      tolerance);
}

TEST_F(GeometryTest, LayoutPriorityKeepsTheHigherPriorityLabelWhole) {
  const composition::Rect whole = natural(title);
  const Laid even =
      laid(components::HStack(text(title, "first"), text(title, "second"))
               .frame({.width = 200.0F}));
  EXPECT_LT(frame(*even.tree, "second").width, whole.width - 1.0F);
  const Laid ranked =
      laid(components::HStack(text(title, "first"),
                              text(title, "second").layout_priority(1.0))
               .frame({.width = 200.0F}));
  EXPECT_NEAR(frame(*ranked.tree, "second").width, whole.width, 0.5F);
  EXPECT_LT(frame(*ranked.tree, "first").width,
            200.0F - whole.width + tolerance);
}

TEST_F(GeometryTest, AHigherPriorityFlexibleViewTakesAllTheSpace) {
  const Laid result =
      laid(components::HStack(
               components::HStackOptions{.spacing = 0.0F},
               components::VStack().frame({.max_width = infinity}).id("low"),
               components::VStack()
                   .frame({.max_width = infinity})
                   .layout_priority(1.0)
                   .id("high"))
               .frame({.width = 200.0F, .height = 10.0F}));
  EXPECT_NEAR(frame(*result.tree, "low").width, 0.0F, tolerance);
  EXPECT_NEAR(frame(*result.tree, "high").width, 200.0F, tolerance);
}

TEST_F(GeometryTest, ALowerPriorityFlexibleViewKeepsItsMinimum) {
  const Laid result = laid(
      components::HStack(components::HStackOptions{.spacing = 0.0F},
                         components::VStack()
                             .frame({.min_width = 20.0F, .max_width = infinity})
                             .id("low"),
                         components::VStack()
                             .frame({.max_width = infinity})
                             .layout_priority(1.0)
                             .id("high"))
          .frame({.width = 200.0F, .height = 10.0F}));
  EXPECT_NEAR(frame(*result.tree, "low").width, 20.0F, tolerance);
  EXPECT_NEAR(frame(*result.tree, "high").width, 180.0F, tolerance);
}

TEST_F(GeometryTest, PositionCentresTheViewOnThePointAndFillsTheProposal) {
  const Laid result = laid(box("dot", 20.0F, 20.0F)
                               .position(50.0F, 30.0F)
                               .id("place")
                               .frame({.width = 200.0F, .height = 100.0F}));
  const composition::Rect dot = offset(*result.tree, "dot", "place");
  EXPECT_NEAR(dot.x, 40.0F, tolerance);
  EXPECT_NEAR(dot.y, 20.0F, tolerance);
  EXPECT_NEAR(frame(*result.tree, "place").width, 200.0F, tolerance);
  EXPECT_NEAR(frame(*result.tree, "place").height, 100.0F, tolerance);
}

TEST_F(GeometryTest, APositionedViewTakesTheRemainingHeightInAStack) {
  const Laid result =
      laid(components::VStack(
               components::VStackOptions{.spacing = 0.0F},
               box("dot", 20.0F, 20.0F).position(10.0F, 10.0F).id("place"),
               box("after", 20.0F, 20.0F))
               .frame({.width = 100.0F, .height = 100.0F})
               .id("column"));
  EXPECT_NEAR(frame(*result.tree, "place").height, 80.0F, tolerance);
  EXPECT_NEAR(offset(*result.tree, "after", "column").y, 80.0F, tolerance);
}

TEST_F(GeometryTest, ZIndexLeavesLayoutUnchanged) {
  const Laid result = laid(components::HStack(
      components::HStackOptions{.spacing = 0.0F},
      box("first", 30.0F, 30.0F).z_index(1.0), box("second", 30.0F, 30.0F)));
  EXPECT_NEAR(offset(*result.tree, "second", "first").x, 30.0F, tolerance);
}

TEST_F(GeometryTest, ContainerRelativeFrameDividesTheScrollViewport) {
  std::vector<composition::Element> cells;
  for (int index = 0; index < 5; ++index) {
    cells.push_back(components::VStack()
                        .container_relative_frame(Axes::horizontal, 3, 1, 10.0F)
                        .frame({.height = 20.0F})
                        .id(std::format("c{}", index)));
  }
  const Laid result =
      laid(components::ScrollView(
               Axes::horizontal,
               components::HStack(components::HStackOptions{.spacing = 10.0F},
                                  std::move(cells)))
               .frame({.width = 320.0F, .height = 50.0F})
               .id("view"));
  EXPECT_NEAR(frame(*result.tree, "c0").width, 100.0F, tolerance);
  EXPECT_NEAR(offset(*result.tree, "c1", "view").x, 110.0F, tolerance);
}

TEST_F(GeometryTest, ContainerRelativeFrameSpansColumns) {
  const Laid result =
      laid(components::ScrollView(
               Axes::horizontal,
               components::HStack(
                   components::HStackOptions{.spacing = 10.0F},
                   components::VStack()
                       .container_relative_frame(Axes::horizontal, 3, 2, 10.0F)
                       .frame({.height = 20.0F})
                       .id("wide"),
                   components::VStack()
                       .container_relative_frame(Axes::horizontal, 3, 1, 10.0F)
                       .frame({.height = 20.0F})
                       .id("narrow")))
               .frame({.width = 320.0F, .height = 50.0F}));
  EXPECT_NEAR(frame(*result.tree, "wide").width, 210.0F, tolerance);
  EXPECT_NEAR(frame(*result.tree, "narrow").width, 100.0F, tolerance);
}

TEST_F(GeometryTest, ContainerRelativeFrameExcludesContentMargins) {
  std::vector<composition::Element> cells;
  for (int index = 0; index < 4; ++index) {
    cells.push_back(components::VStack()
                        .container_relative_frame(Axes::horizontal, 3, 1, 10.0F)
                        .frame({.height = 20.0F})
                        .id(std::format("c{}", index)));
  }
  const Laid result =
      laid(components::ScrollView(
               Axes::horizontal,
               components::HStack(components::HStackOptions{.spacing = 10.0F},
                                  std::move(cells)))
               .content_margins(theme::Edge::horizontal, 20.0F)
               .frame({.width = 320.0F, .height = 50.0F})
               .id("view"));
  EXPECT_NEAR(frame(*result.tree, "c0").width, 260.0F / 3.0F, tolerance);
  EXPECT_NEAR(offset(*result.tree, "c0", "view").x, 20.0F, tolerance);
}

TEST_F(GeometryTest, IgnoresSafeAreaLetsAScrollViewUseTheInsets) {
  const auto inset = [](bool ignores) {
    composition::Element scroller =
        components::ScrollView(components::VStack()
                                   .frame({.width = 40.0F, .height = 20.0F})
                                   .id("first"))
            .frame({.max_width = infinity, .max_height = infinity});
    if (ignores) {
      scroller = std::move(scroller).ignores_safe_area(theme::Edge::top);
    }
    const Laid result =
        laid(components::VStack(std::move(scroller).id("view"))
                 .frame({.max_width = infinity,
                         .max_height = infinity,
                         .alignment = composition::Alignment::top_leading})
                 .environment(composition::SafeArea::insets,
                              theme::Edges{.top = 25.0F}));
    const auto view =
        result.tree->node(*result.tree->find(composition::ElementKey{"view"}));
    return std::pair{view->get().safe_area().top,
                     offset(*result.tree, "first", "view").y};
  };
  const auto [kept, kept_offset] = inset(false);
  const auto [ignored, ignored_offset] = inset(true);
  EXPECT_NEAR(kept, 25.0F, tolerance);
  EXPECT_NEAR(kept_offset, 25.0F, tolerance);
  EXPECT_NEAR(ignored, 0.0F, tolerance);
  EXPECT_NEAR(ignored_offset, 0.0F, tolerance);
}

TEST_F(GeometryTest, SafeAreaPaddingInsetsScrollContent) {
  const Laid result =
      laid(components::ScrollView(components::VStack()
                                      .frame({.width = 40.0F, .height = 20.0F})
                                      .id("first"))
               .safe_area_padding(theme::Edge::top, theme::Space{30.0F})
               .frame({.width = 100.0F, .height = 200.0F})
               .id("view"));
  EXPECT_NEAR(offset(*result.tree, "first", "view").y, 30.0F, tolerance);
}

TEST_F(GeometryTest, ContainerRelativeFrameTakesALengthFunction) {
  const Laid result = laid(
      components::ScrollView(
          Axes::horizontal,
          components::VStack()
              .container_relative_frame(
                  Axes::horizontal, Alignment::center,
                  [](float length, composition::Axis) { return length * 0.5F; })
              .frame({.height = 20.0F})
              .id("c"))
          .frame({.width = 300.0F, .height = 200.0F})
          .id("view"));
  EXPECT_NEAR(frame(*result.tree, "c").width, 150.0F, tolerance);
  EXPECT_NEAR(offset(*result.tree, "c", "view").x, 0.0F, tolerance);
}

TEST_F(GeometryTest, ContainerRelativeFrameFallsBackToTheWindow) {
  const Laid result =
      laid(components::VStack()
               .container_relative_frame(Axes::horizontal, 4, 1, 0.0F)
               .frame({.height = 20.0F})
               .id("quarter"));
  EXPECT_NEAR(frame(*result.tree, "quarter").width, 320.0F, tolerance);
}

TEST_F(GeometryTest, GeometryReaderBuildsItsBodyFromItsSize) {
  const Laid result = laid(
      components::GeometryReader(
          [](const composition::GeometryProxy& proxy) -> composition::Element {
            return components::VStack()
                .frame({.width = proxy.size().width / 2.0F, .height = 10.0F})
                .id("child");
          })
          .id("reader")
          .frame({.width = 200.0F, .height = 100.0F}));
  const composition::Rect child = offset(*result.tree, "child", "reader");
  EXPECT_NEAR(child.width, 100.0F, tolerance);
  EXPECT_NEAR(child.x, 0.0F, tolerance);
  EXPECT_NEAR(child.y, 0.0F, tolerance);
}

TEST_F(GeometryTest, GeometryReaderTakesTheRemainingSpaceInAStack) {
  const Laid result = laid(
      components::VStack(
          components::VStackOptions{.spacing = 0.0F}, box("top", 40.0F, 30.0F),
          components::GeometryReader([](const composition::GeometryProxy&)
                                         -> composition::Element {
            return components::VStack();
          }).id("reader"))
          .frame({.width = 200.0F, .height = 100.0F}));
  EXPECT_NEAR(frame(*result.tree, "reader").width, 200.0F, tolerance);
  EXPECT_NEAR(frame(*result.tree, "reader").height, 70.0F, tolerance);
}

TEST_F(GeometryTest, GeometryReaderInAScrollViewIsTenPointsAlongTheScroll) {
  const Laid result =
      laid(components::ScrollView(
               components::GeometryReader([](const composition::GeometryProxy&)
                                              -> composition::Element {
                 return components::VStack();
               }).id("reader"))
               .frame({.width = 200.0F, .height = 100.0F}));
  EXPECT_NEAR(frame(*result.tree, "reader").width, 200.0F, tolerance);
  EXPECT_NEAR(frame(*result.tree, "reader").height, 10.0F, tolerance);
}

TEST_F(GeometryTest, GeometryProxyReportsANamedCoordinateSpace) {
  auto seen = std::make_shared<std::optional<composition::Rect>>();
  const Laid result = laid(
      components::VStack(
          components::VStackOptions{.spacing = 0.0F}, box("gap", 10.0F, 30.0F),
          components::GeometryReader([seen](const composition::GeometryProxy&
                                                proxy) -> composition::Element {
            *seen = proxy.frame(composition::CoordinateSpace::named("outer"));
            return components::VStack();
          }).frame({.width = 100.0F, .height = 20.0F}))
          .padding(7.0F)
          .coordinate_space("outer"));
  ASSERT_TRUE(seen->has_value());
  EXPECT_NEAR((*seen)->y, 37.0F, tolerance);
  EXPECT_NEAR((*seen)->x, 7.0F, tolerance);
}

TEST_F(GeometryTest, AWrappedTextReportsItsWidestLine) {
  const Laid result = laid(components::Text(std::string{paragraph})
                               .frame({.width = 300.0F})
                               .id("text"));
  const auto found = result.tree->find(composition::ElementKey{"text"});
  ASSERT_TRUE(found.has_value());
  const std::optional<composition::Rect> inside =
      result.tree->node(*found)->get().content_frame();
  ASSERT_TRUE(inside.has_value());
  EXPECT_LT(inside->width, 300.0F - 1.0F);
  EXPECT_NEAR(inside->x, (300.0F - inside->width) * 0.5F, tolerance);
}

TEST_F(GeometryTest, AWrappedTextInALeadingStackIsNarrowerThanTheStack) {
  const Laid result =
      laid(components::VStack(
               components::VStackOptions{
                   .alignment = components::HorizontalAlignment::leading,
                   .spacing = 0.0F},
               components::Text(std::string{paragraph}).id("text"),
               box("bar", 200.0F, 4.0F))
               .frame({.width = 200.0F})
               .id("column"));
  EXPECT_LT(frame(*result.tree, "text").width, 200.0F - 1.0F);
  EXPECT_NEAR(frame(*result.tree, "column").width, 200.0F, tolerance);
}

TEST_F(GeometryTest, GridRowPaddingAppliesToEachCell) {
  const Laid result = laid(
      components::Grid(
          components::GridOptions{.horizontal_spacing = 0.0F,
                                  .vertical_spacing = 0.0F},
          components::GridRow(box("a", 30.0F, 20.0F), box("b", 40.0F, 20.0F))
              .padding(10.0F),
          components::GridRow(box("c", 30.0F, 20.0F), box("d", 40.0F, 20.0F)))
          .id("grid"));
  const composition::Rect grid = frame(*result.tree, "grid");
  EXPECT_NEAR(grid.width, 110.0F, tolerance);
  EXPECT_NEAR(grid.height, 60.0F, tolerance);
  EXPECT_NEAR(offset(*result.tree, "a", "grid").x, 10.0F, tolerance);
  EXPECT_NEAR(offset(*result.tree, "a", "grid").y, 10.0F, tolerance);
  EXPECT_NEAR(offset(*result.tree, "b", "grid").x, 60.0F, tolerance);
  EXPECT_NEAR(offset(*result.tree, "c", "grid").x, 10.0F, tolerance);
  EXPECT_NEAR(offset(*result.tree, "c", "grid").y, 40.0F, tolerance);
  EXPECT_NEAR(offset(*result.tree, "d", "grid").x, 60.0F, tolerance);
}

TEST_F(GeometryTest, GridRowFrameAppliesToEachCell) {
  const Laid result = laid(
      components::Grid(
          components::GridOptions{.horizontal_spacing = 0.0F,
                                  .vertical_spacing = 0.0F},
          components::GridRow(box("a", 30.0F, 20.0F), box("b", 40.0F, 20.0F))
              .frame({.height = 50.0F}),
          components::GridRow(box("c", 30.0F, 20.0F), box("d", 40.0F, 20.0F)))
          .id("grid"));
  const composition::Rect grid = frame(*result.tree, "grid");
  EXPECT_NEAR(grid.width, 70.0F, tolerance);
  EXPECT_NEAR(grid.height, 70.0F, tolerance);
  EXPECT_NEAR(offset(*result.tree, "a", "grid").y, 15.0F, tolerance);
  EXPECT_NEAR(offset(*result.tree, "b", "grid").x, 30.0F, tolerance);
  EXPECT_NEAR(offset(*result.tree, "c", "grid").y, 50.0F, tolerance);
}

TEST_F(GeometryTest, StacksTakeTheFlexibilityOfTheirChildren) {
  const auto colour = [](composition::FrameSpec frame) {
    if (!frame.width.has_value()) {
      frame.ideal_width = 10.0F;
      frame.max_width = infinity;
    }
    if (!frame.height.has_value()) {
      frame.ideal_height = 10.0F;
      frame.max_height = infinity;
    }
    return components::ZStack().frame(frame);
  };
  const auto size = [](const composition::Tree& built, std::string_view key) {
    const composition::Rect found = frame(built, key);
    return std::array{found.width, found.height};
  };
  const auto expect =
      [&](composition::Element content, std::string_view case_name,
          std::vector<std::pair<std::string_view, std::array<float, 2>>>
              wanted) {
        const Laid result = laid(std::move(content));
        for (const auto& [key, extent] : wanted) {
          const std::array<float, 2> got = size(*result.tree, key);
          EXPECT_NEAR(got[0], extent[0], tolerance) << case_name << " " << key;
          EXPECT_NEAR(got[1], extent[1], tolerance) << case_name << " " << key;
        }
      };
  expect(components::ScrollView(
             components::VStack(colour({}).id("child")).id("stack"))
             .frame({.width = 200.0F, .height = 300.0F}),
         "scroll_vstack",
         {{"stack", {200.0F, 10.0F}}, {"child", {200.0F, 10.0F}}});
  expect(components::ScrollView(
             components::ZStack(colour({}).id("child")).id("stack"))
             .frame({.width = 200.0F, .height = 300.0F}),
         "scroll_zstack",
         {{"stack", {200.0F, 10.0F}}, {"child", {200.0F, 10.0F}}});
  expect(components::ScrollView(
             Axes::horizontal,
             components::HStack(components::VStack(colour({})).id("stack")))
             .frame({.width = 200.0F, .height = 100.0F}),
         "hscroll_vstack", {{"stack", {10.0F, 100.0F}}});
  expect(components::HStack(components::HStackOptions{.spacing = 0.0F},
                            components::VStack(colour({})).id("stack"),
                            colour({.width = 50.0F}))
             .frame({.width = 200.0F, .height = 40.0F}),
         "row_vstack", {{"stack", {150.0F, 40.0F}}});
  expect(components::VStack(components::VStackOptions{.spacing = 0.0F},
                            colour({.height = 30.0F}),
                            components::ZStack(colour({})).id("stack"))
             .frame({.width = 100.0F, .height = 100.0F}),
         "column_zstack", {{"stack", {100.0F, 70.0F}}});
  expect(components::HStack(
             components::HStackOptions{.spacing = 0.0F},
             components::VStack(box("square", 30.0F, 10.0F)).id("rigid"),
             components::VStack(colour({})).id("stack"))
             .frame({.width = 200.0F, .height = 40.0F}),
         "mixed", {{"rigid", {30.0F, 10.0F}}, {"stack", {170.0F, 40.0F}}});
  expect(
      components::VStack(
          components::VStackOptions{.spacing = 0.0F}, colour({.height = 30.0F}),
          components::VStack(components::HStack(colour({}))).id("stack"))
          .frame({.width = 100.0F, .height = 100.0F}),
      "nested", {{"stack", {100.0F, 70.0F}}});
}

}

namespace arisa::layout {

TEST_F(GeometryTest, SpacersDividersAndButtonsMakeTheirStackFlexible) {
  const auto measure = [](composition::Element row) {
    const Laid result =
        laid(components::VStack(components::VStackOptions{.spacing = 0.0F},
                                std::move(row).id("stack"))
                 .frame({.width = 200.0F, .height = 100.0F}));
    return frame(*result.tree, "stack");
  };
  const composition::Rect spaced =
      measure(components::HStack(components::HStackOptions{.spacing = 0.0F},
                                 box("a", 30.0F, 10.0F), components::Spacer()));
  EXPECT_NEAR(spaced.width, 200.0F, tolerance);
  EXPECT_NEAR(spaced.height, 10.0F, tolerance);
  const composition::Rect divided = measure(
      components::HStack(components::HStackOptions{.spacing = 0.0F},
                         box("a", 30.0F, 10.0F), components::Divider()));
  EXPECT_NEAR(divided.width, 31.0F, tolerance);
  EXPECT_NEAR(divided.height, 100.0F, tolerance);
  const composition::Rect pressed = measure(components::HStack(
      components::HStackOptions{.spacing = 0.0F},
      components::Button([] {}, components::ZStack()
                                    .frame({.max_width = infinity})
                                    .frame({.height = 10.0F}))
          .button_style(components::ButtonStyle::plain())));
  EXPECT_NEAR(pressed.width, 200.0F, tolerance);
  EXPECT_NEAR(pressed.height, 10.0F, tolerance);
}

TEST_F(GeometryTest, AShortLabelKeepsItsWidthWithoutPriorities) {
  const composition::Rect price = natural("$9.99");
  const Laid result =
      laid(components::HStack(text(title, "title"), text("$9.99", "price"))
               .frame({.width = 160.0F})
               .id("row"));
  const composition::Rect shown = frame(*result.tree, "price");
  EXPECT_NEAR(shown.width, price.width, tolerance);
  const composition::Rect named = frame(*result.tree, "title");
  EXPECT_LE(named.x + named.width, shown.x + tolerance);
  EXPECT_LE(shown.x + shown.width,
            frame(*result.tree, "row").x + 160.0F + tolerance);
}

TEST_F(GeometryTest, AHigherPriorityParagraphKeepsItsLinesInAColumn) {
  const composition::Rect line = natural("A");
  const Laid result = laid(
      components::VStack(components::VStackOptions{.spacing = 0.0F},
                         components::Text(std::string{paragraph}).id("low"),
                         components::Text(std::string{paragraph})
                             .layout_priority(1.0)
                             .id("high"))
          .frame({.width = 120.0F, .height = 80.0F}));
  const composition::Rect low = frame(*result.tree, "low");
  const composition::Rect high = frame(*result.tree, "high");
  EXPECT_NEAR(low.height, line.height, tolerance);
  EXPECT_NEAR(high.height, 80.0F - line.height, tolerance);
}

TEST_F(GeometryTest, EqualPriorityParagraphsShareAShortColumnInOrder) {
  const float line = natural("A").height;
  const Laid result =
      laid(components::VStack(components::VStackOptions{.spacing = 0.0F},
                              components::Text(std::string{paragraph}).id("a"),
                              components::Text(std::string{paragraph}).id("b"))
               .frame({.width = 100.0F, .height = 60.0F})
               .id("frame"));
  const float first = std::max(std::floor(30.0F / line), 1.0F) * line;
  const float second = std::floor((60.0F - first) / line) * line;
  const composition::Rect outer = frame(*result.tree, "frame");
  const composition::Rect a = frame(*result.tree, "a");
  const composition::Rect b = frame(*result.tree, "b");
  EXPECT_NEAR(a.height, first, tolerance);
  EXPECT_NEAR(b.height, second, tolerance);
  EXPECT_NEAR(a.y - outer.y, (60.0F - first - second) * 0.5F, tolerance);
  EXPECT_NEAR(b.y, a.y + a.height, tolerance);
  const Laid uneven =
      laid(components::VStack(components::VStackOptions{.spacing = 0.0F},
                              components::Text("one two").id("a"),
                              components::Text(std::string{paragraph}).id("b"))
               .frame({.width = 100.0F, .height = 60.0F})
               .id("frame"));
  EXPECT_NEAR(frame(*uneven.tree, "a").height, line, tolerance);
  EXPECT_NEAR(frame(*uneven.tree, "b").height,
              std::floor((60.0F - line) / line) * line, tolerance);
}

struct GeometryShift {
  float x{0.0F};

  auto effect_value(composition::Size) const -> graphics::ProjectionTransform {
    return graphics::ProjectionTransform{
        graphics::AffineTransform::translation(x, 0.0F)};
  }

  friend auto operator==(const GeometryShift&, const GeometryShift&)
      -> bool = default;
};

class GrowingBox final : public composition::Component {
 public:
  GrowingBox(std::shared_ptr<composition::Observable<float>> width,
             std::shared_ptr<std::vector<std::string>> calls)
      : width_(std::move(width)), calls_(std::move(calls)) {}

  auto operator==(const GrowingBox&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override {
    return LayoutFixture::box("grow", context.read(*width_), 10.0F)
        .on_geometry_change(
            std::type_identity<float>{},
            [](const composition::GeometryProxy& proxy) {
              return proxy.size().width;
            },
            [calls = calls_](float old_width, float new_width) {
              calls->push_back(std::format("{}->{}", old_width, new_width));
            });
  }

 private:
  std::shared_ptr<composition::Observable<float>> width_;
  std::shared_ptr<std::vector<std::string>> calls_;
};

TEST_F(GeometryTest, GeometryEffectsMoveChildGeometryUnlessIgnoredByLayout) {
  auto seen = std::make_shared<std::array<float, 3>>();
  const auto probe = [seen](std::string_view key, std::size_t slot) {
    return box(key, 20.0F, 10.0F)
        .on_geometry_change(
            std::type_identity<float>{},
            [](const composition::GeometryProxy& proxy) {
              return proxy.frame(composition::CoordinateSpace::global).x;
            },
            [seen, slot](float x) { seen->at(slot) = x; });
  };
  const Laid result =
      laid(components::HStack(
               components::HStackOptions{
                   .alignment = components::VerticalAlignment::top,
                   .spacing = theme::Space{0.0F}},
               components::VStack(probe("plain", 0)),
               components::VStack(probe("shifted", 1))
                   .geometry_effect(GeometryShift{.x = 30.0F}),
               components::VStack(probe("ignored", 2))
                   .geometry_effect(composition::IgnoredByLayoutEffect{
                       GeometryShift{.x = 30.0F}}))
               .fixed_size()
               .id("row"));
  const float origin = frame(*result.tree, "row").x;
  EXPECT_NEAR(seen->at(0) - origin, 0.0F, tolerance);
  EXPECT_NEAR(seen->at(1) - origin, 50.0F, tolerance);
  EXPECT_NEAR(seen->at(2) - origin, 40.0F, tolerance);
}

TEST_F(GeometryTest, AnOldAndNewGeometryActionFiresInitiallyThenOnChange) {
  auto width = std::make_shared<composition::Observable<float>>(40.0F);
  auto calls = std::make_shared<std::vector<std::string>>();
  Laid result = laid(composition::Element{GrowingBox{width, calls}});
  ASSERT_EQ(*calls, (std::vector<std::string>{"40->40"}));
  width->set(60.0F);
  result.tree->reconcile();
  ASSERT_TRUE(
      result.context.update(*result.tree, handheld, std::ref(typesetter()))
          .has_value());
  EXPECT_EQ(*calls, (std::vector<std::string>{"40->40", "40->60"}));
}

TEST_F(GeometryTest, GeometryReadersQueryTheDeclaredReservedRegions) {
  using composition::ReservedRegion;
  auto found = std::make_shared<std::vector<ReservedRegion>>();
  auto mirrored = std::make_shared<std::vector<ReservedRegion>>();
  const auto reader = [](std::shared_ptr<std::vector<ReservedRegion>> into) {
    return components::GeometryReader(
               [into](const composition::GeometryProxy& proxy) {
                 *into =
                     proxy.reserved_regions(ReservedRegion::Kind::occlusion);
                 return composition::Element{components::VStack()};
               })
        .frame({.width = 200.0F, .height = 100.0F});
  };
  auto built =
      tree(components::VStack(
               components::VStackOptions{
                   .alignment = components::HorizontalAlignment::leading,
                   .spacing = theme::Space{0.0F}},
               reader(found).id("ltr"),
               reader(mirrored)
                   .environment(components::Environment::layout_direction,
                                composition::LayoutDirection::right_to_left)
                   .id("rtl"))
               .frame({.width = 1280.0F,
                       .height = 720.0F,
                       .alignment = Alignment::top_leading}));
  auto context = LayoutContext::create();
  ASSERT_TRUE(context.has_value());
  ASSERT_TRUE(
      context->update(*built, handheld, std::ref(typesetter())).has_value());
  const composition::Rect first = frame(*built, "ltr");
  const composition::Rect second = frame(*built, "rtl");
  EXPECT_TRUE(found->empty());
  built->declare_reserved_regions(
      {ReservedRegion{ReservedRegion::ID{7U}, ReservedRegion::Kind::occlusion,
                      composition::Rect{.x = first.x + 10.0F,
                                        .y = first.y + 5.0F,
                                        .width = 30.0F,
                                        .height = 20.0F}},
       ReservedRegion{ReservedRegion::ID{8U}, ReservedRegion::Kind::occlusion,
                      composition::Rect{.x = second.x + 10.0F,
                                        .y = second.y + 5.0F,
                                        .width = 30.0F,
                                        .height = 20.0F}}});
  for (int round = 0; round < 3; ++round) {
    built->reconcile();
    ASSERT_TRUE(
        context->update(*built, handheld, std::ref(typesetter())).has_value());
  }
  ASSERT_EQ(found->size(), 1U);
  EXPECT_EQ(found->front().id(), ReservedRegion::ID{7U});
  EXPECT_NEAR(found->front().frame().x, 10.0F, tolerance);
  EXPECT_NEAR(found->front().frame().y, 5.0F, tolerance);
  ASSERT_EQ(mirrored->size(), 1U);
  EXPECT_EQ(mirrored->front().id(), ReservedRegion::ID{8U});
  EXPECT_NEAR(mirrored->front().frame().x, 160.0F, tolerance);
}

TEST_F(GeometryTest, NegativeStackSpacingOverlapsTheChildren) {
  const Laid row =
      laid(components::HStack(components::HStackOptions{.spacing = -10.0F},
                              box("a", 20.0F, 10.0F), box("b", 20.0F, 10.0F))
               .id("row"));
  EXPECT_NEAR(frame(*row.tree, "row").width, 30.0F, tolerance);
  EXPECT_NEAR(offset(*row.tree, "a", "row").x, 0.0F, tolerance);
  EXPECT_NEAR(offset(*row.tree, "b", "row").x, 10.0F, tolerance);
  const Laid column =
      laid(components::VStack(components::VStackOptions{.spacing = -10.0F},
                              box("a", 20.0F, 30.0F), box("b", 20.0F, 30.0F))
               .id("column"));
  EXPECT_NEAR(frame(*column.tree, "column").height, 50.0F, tolerance);
  EXPECT_NEAR(offset(*column.tree, "b", "column").y, 20.0F, tolerance);
}

TEST_F(GeometryTest, NegativePaddingShrinksTheFootprintAndOverflows) {
  const Laid result =
      laid(components::HStack(components::HStackOptions{.spacing = 0.0F},
                              box("leading", 10.0F, 10.0F),
                              box("inner", 40.0F, 20.0F).padding(-4.0F),
                              box("trailing", 10.0F, 10.0F))
               .id("row"));
  EXPECT_NEAR(frame(*result.tree, "row").width, 52.0F, tolerance);
  EXPECT_NEAR(frame(*result.tree, "row").height, 12.0F, tolerance);
  const composition::Rect inner = offset(*result.tree, "inner", "row");
  EXPECT_NEAR(inner.x, 6.0F, tolerance);
  EXPECT_NEAR(inner.y, -4.0F, tolerance);
  EXPECT_NEAR(inner.width, 40.0F, tolerance);
  EXPECT_NEAR(offset(*result.tree, "leading", "row").y, 1.0F, tolerance);
  EXPECT_NEAR(offset(*result.tree, "trailing", "row").x, 42.0F, tolerance);
  const Laid sides = laid(
      components::HStack(
          components::HStackOptions{.spacing = 0.0F},
          box("inner", 40.0F, 20.0F).padding(theme::Edge::horizontal, -4.0F))
          .id("row"));
  EXPECT_NEAR(frame(*sides.tree, "row").width, 32.0F, tolerance);
  EXPECT_NEAR(frame(*sides.tree, "row").height, 20.0F, tolerance);
  EXPECT_NEAR(offset(*sides.tree, "inner", "row").x, -4.0F, tolerance);
}

TEST_F(GeometryTest, ScenePaddingAddsTheMinimumSceneMargin) {
  const Laid all = laid(components::VStack(box("inner", 40.0F, 20.0F))
                            .scene_padding()
                            .id("outer"));
  EXPECT_NEAR(frame(*all.tree, "outer").width, 80.0F, tolerance);
  EXPECT_NEAR(frame(*all.tree, "outer").height, 60.0F, tolerance);
  EXPECT_NEAR(offset(*all.tree, "inner", "outer").x, 20.0F, tolerance);
  EXPECT_NEAR(offset(*all.tree, "inner", "outer").y, 20.0F, tolerance);
  const Laid sides = laid(components::VStack(box("inner", 40.0F, 20.0F))
                              .scene_padding(composition::ScenePadding::minimum,
                                             theme::Edge::horizontal)
                              .id("outer"));
  EXPECT_NEAR(frame(*sides.tree, "outer").width, 80.0F, tolerance);
  EXPECT_NEAR(frame(*sides.tree, "outer").height, 20.0F, tolerance);
}

TEST_F(GeometryTest, IgnoringTheContainerRegionExtendsIntoTheSafeArea) {
  const auto placed = [](composition::SafeAreaRegions regions) {
    const Laid result =
        laid(components::VStack(
                 components::VStack()
                     .frame({.max_width = infinity, .max_height = infinity})
                     .background(theme::ColourRole::accent,
                                 theme::Shape::rectangle())
                     .ignores_safe_area(regions, theme::Edge::top)
                     .id("fill"))
                 .frame({.width = 200.0F, .height = 100.0F})
                 .environment(composition::SafeArea::insets,
                              theme::Edges{.top = 20.0F})
                 .id("view"));
    return offset(*result.tree, "fill", "view");
  };
  EXPECT_NEAR(placed(composition::SafeAreaRegions::container).y, 0.0F,
              tolerance);
  EXPECT_NEAR(placed(composition::SafeAreaRegions::container).height, 100.0F,
              tolerance);
  EXPECT_NEAR(placed(composition::SafeAreaRegions::keyboard).y, 20.0F,
              tolerance);
  EXPECT_NEAR(placed(composition::SafeAreaRegions::keyboard).height, 80.0F,
              tolerance);
  EXPECT_NEAR(placed(composition::SafeAreaRegions::all).y, 0.0F, tolerance);
}

TEST_F(GeometryTest, IgnoredSafeAreaAlignmentPlacesTheContent) {
  const auto placed = [](std::optional<Alignment> alignment) {
    const Laid result =
        laid(components::VStack(
                 components::VStackOptions{.spacing = 0.0F},
                 box("box", 20.0F, 20.0F)
                     .ignores_safe_area(composition::SafeAreaRegions::all,
                                        theme::Edge::top, alignment),
                 components::Spacer())
                 .frame({.width = 200.0F, .height = 100.0F})
                 .environment(composition::SafeArea::insets,
                              theme::Edges{.top = 40.0F})
                 .id("view"));
    return offset(*result.tree, "box", "view").y;
  };
  EXPECT_NEAR(placed(std::nullopt), 0.0F, tolerance);
  EXPECT_NEAR(placed(Alignment::top), 0.0F, tolerance);
  EXPECT_NEAR(placed(Alignment::center), 20.0F, tolerance);
  EXPECT_NEAR(placed(Alignment::bottom), 40.0F, tolerance);
}

TEST_F(GeometryTest, GeometryProxyReportsTheSafeAreaInsets) {
  const auto inset = [](bool ignores) {
    auto seen = std::make_shared<theme::Edges>();
    composition::Element reader =
        components::GeometryReader([seen](
                                       const composition::GeometryProxy& proxy)
                                       -> composition::Element {
          *seen = proxy.safe_area_insets();
          return components::VStack();
        }).id("reader");
    if (ignores) {
      reader = std::move(reader).ignores_safe_area();
    }
    const Laid result =
        laid(components::VStack(std::move(reader))
                 .frame({.width = 200.0F, .height = 100.0F})
                 .environment(composition::SafeArea::insets,
                              theme::Edges{.top = 20.0F, .leading = 10.0F}));
    return *seen;
  };
  const theme::Edges kept = inset(false);
  EXPECT_NEAR(kept.top, 20.0F, tolerance);
  EXPECT_NEAR(kept.leading, 10.0F, tolerance);
  EXPECT_NEAR(kept.bottom, 0.0F, tolerance);
  const theme::Edges ignored = inset(true);
  EXPECT_NEAR(ignored.top, 0.0F, tolerance);
  EXPECT_NEAR(ignored.leading, 0.0F, tolerance);
}

TEST_F(GeometryTest, GeometryProxyReportsContainerCornerInsets) {
  struct Seen {
    theme::RectangleCornerInsets insets;
    std::optional<theme::RectangleCornerRadii> radii;
    std::optional<theme::RectangleCornerRadii> within;
  };
  const auto corner = [](float leading, float top, theme::Shape shape,
                         composition::Size outer, composition::Size inner,
                         Alignment alignment) {
    auto seen = std::make_shared<Seen>();
    const Laid result =
        laid(components::ZStack(
                 components::ZStackOptions{.alignment = alignment},
                 components::ZStack(
                     components::GeometryReader(
                         [seen](const composition::GeometryProxy& proxy)
                             -> composition::Element {
                           *seen =
                               Seen{.insets = proxy.container_corner_insets(),
                                    .radii = proxy.concentric_corner_radii(),
                                    .within = proxy.concentric_corner_radii(
                                        composition::Rect{.width = 20.0F,
                                                          .height = 20.0F})};
                           return components::VStack();
                         })
                         .frame({.width = inner.width, .height = inner.height}))
                     .padding(theme::Insets{.top = top, .leading = leading}))
                 .frame({.width = outer.width,
                         .height = outer.height,
                         .alignment = alignment})
                 .container_shape(shape));
    return *seen;
  };
  const composition::Size wide{.width = 200.0F, .height = 100.0F};
  const composition::Size square{.width = 40.0F, .height = 40.0F};
  const theme::Shape rounded = theme::RoundedRectangle(20.0F);
  const Seen near =
      corner(4.0F, 4.0F, rounded, wide, square, Alignment::top_leading);
  EXPECT_NEAR(near.insets.top_leading.width, 16.0F, tolerance);
  EXPECT_NEAR(near.insets.top_leading.height, 16.0F, tolerance);
  EXPECT_NEAR(near.insets.top_trailing.width, 0.0F, tolerance);
  EXPECT_NEAR(near.insets.bottom_leading.height, 0.0F, tolerance);
  ASSERT_TRUE(near.radii.has_value());
  EXPECT_NEAR(near.radii->top_leading, 16.0F, tolerance);
  EXPECT_NEAR(near.radii->bottom_trailing, 0.0F, tolerance);
  ASSERT_TRUE(near.within.has_value());
  EXPECT_NEAR(near.within->top_leading, 10.0F, tolerance);
  EXPECT_NEAR(corner(4.0F, 10.0F, rounded, wide, square, Alignment::top_leading)
                  .insets.top_leading.width,
              10.0F, tolerance);
  EXPECT_NEAR(corner(10.0F, 4.0F, rounded, wide, square, Alignment::top_leading)
                  .insets.top_leading.height,
              10.0F, tolerance);
  EXPECT_NEAR(corner(30.0F, 4.0F, rounded, wide, square, Alignment::top_leading)
                  .insets.top_leading.width,
              0.0F, tolerance);
  const Seen capsule = corner(
      0.0F, 0.0F, theme::Capsule(),
      composition::Size{.width = 60.0F, .height = 30.0F},
      composition::Size{.width = 40.0F, .height = 20.0F}, Alignment::center);
  EXPECT_NEAR(capsule.insets.top_leading.width, 5.0F, tolerance);
  EXPECT_NEAR(capsule.insets.bottom_trailing.height, 5.0F, tolerance);
  ASSERT_TRUE(capsule.radii.has_value());
  EXPECT_NEAR(capsule.radii->top_trailing, 5.0F, tolerance);
}

TEST_F(GeometryTest, GeometryProxyWithoutAContainerHasSquareCorners) {
  auto seen = std::make_shared<std::optional<theme::RectangleCornerRadii>>();
  auto insets = std::make_shared<theme::RectangleCornerInsets>();
  const Laid result = laid(
      components::GeometryReader([seen, insets](
                                     const composition::GeometryProxy& proxy)
                                     -> composition::Element {
        *seen = proxy.concentric_corner_radii();
        *insets = proxy.container_corner_insets();
        return components::VStack();
      }).frame({.width = 40.0F, .height = 20.0F}));
  ASSERT_TRUE(seen->has_value());
  EXPECT_EQ(**seen, theme::RectangleCornerRadii{});
  EXPECT_EQ(*insets, theme::RectangleCornerInsets{});
}

TEST_F(GeometryTest, ContainerCornerOffsetMovesClearOfTheCorner) {
  const auto placed = [](theme::Edge edges, bool fit) {
    const Laid result =
        laid(components::ZStack(
                 components::ZStackOptions{.alignment = Alignment::top_leading},
                 components::VStack()
                     .frame(fit ? composition::FrameSpec{.height = 30.0F,
                                                         .max_width = infinity}
                                : composition::FrameSpec{.width = 30.0F,
                                                         .height = 30.0F})
                     .background(theme::ColourRole::accent,
                                 theme::Shape::rectangle())
                     .container_corner_offset(edges, fit)
                     .id("box"))
                 .frame({.width = 100.0F,
                         .height = 100.0F,
                         .alignment = Alignment::top_leading})
                 .container_shape(theme::RoundedRectangle(20.0F))
                 .id("view"));
    return offset(*result.tree, "box", "view");
  };
  const composition::Rect leading = placed(theme::Edge::leading, false);
  EXPECT_NEAR(leading.x, 20.0F, tolerance);
  EXPECT_NEAR(leading.y, 0.0F, tolerance);
  EXPECT_NEAR(leading.width, 30.0F, tolerance);
  const composition::Rect top = placed(theme::Edge::top, false);
  EXPECT_NEAR(top.x, 0.0F, tolerance);
  EXPECT_NEAR(top.y, 20.0F, tolerance);
  const composition::Rect fitted = placed(theme::Edge::leading, true);
  EXPECT_NEAR(fitted.x, 20.0F, tolerance);
  EXPECT_NEAR(fitted.width, 80.0F, tolerance);
}

TEST_F(GeometryTest, APercentFrameUnderADecorationResolvesAgainstItsParent) {
  const Laid result = laid(
      components::VStack(
          components::VStack()
              .frame({.width = composition::Length::percent(0.5F),
                      .height = 40.0F})
              .id("base")
              .background(Alignment::leading, box("decoration", 10.0F, 10.0F)))
          .frame({.width = 200.0F, .height = 100.0F}));
  EXPECT_NEAR(frame(*result.tree, "base").width, 100.0F, tolerance);
  EXPECT_NEAR(frame(*result.tree, "base").height, 40.0F, tolerance);
  EXPECT_NEAR(frame(*result.tree, "decoration").x,
              frame(*result.tree, "base").x, tolerance);
}

}
