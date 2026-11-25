#include <arisa/components/environment.hpp>
#include <arisa/components/geometry_reader.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/scroll_view_reader.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/composition/alignment.hpp>
#include <arisa/composition/alignment_id.hpp>
#include <arisa/composition/alignment_key.hpp>
#include <arisa/composition/axis.hpp>
#include <arisa/composition/container_corners.hpp>
#include <arisa/composition/edge_corner.hpp>
#include <arisa/composition/edge_corners.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/geometry_effect.hpp>
#include <arisa/composition/geometry_proxy.hpp>
#include <arisa/composition/horizontal_edge.hpp>
#include <arisa/composition/horizontal_edges.hpp>
#include <arisa/composition/interface_orientation.hpp>
#include <arisa/composition/layout_direction_behavior.hpp>
#include <arisa/composition/list_row_separator_alignment.hpp>
#include <arisa/composition/reserved_region.hpp>
#include <arisa/composition/safe_area_regions.hpp>
#include <arisa/composition/scene_padding.hpp>
#include <arisa/composition/scroll_geometry.hpp>
#include <arisa/composition/scroll_phase.hpp>
#include <arisa/composition/scroll_position.hpp>
#include <arisa/composition/vertical_edge.hpp>
#include <arisa/composition/vertical_edges.hpp>
#include <arisa/graphics/any_shape.hpp>
#include <arisa/graphics/offset_shape.hpp>
#include <arisa/graphics/path.hpp>
#include <arisa/graphics/projection_transform.hpp>
#include <arisa/graphics/rotated_shape.hpp>
#include <arisa/graphics/scaled_shape.hpp>
#include <arisa/graphics/transformed_shape.hpp>
#include <arisa/text/drawing_options.hpp>
#include <arisa/text/text_layout.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>

#include <gtest/gtest.h>

#include <array>
#include <optional>
#include <type_traits>
#include <vector>

namespace arisa::composition {

class LayoutTypesTest : public ::testing::Test {
 protected:
  class Wedge {
   public:
    auto path(Rect rect) const -> graphics::Path {
      graphics::Path made;
      made.move(Point{.x = rect.x, .y = rect.y});
      made.add_line(Point{.x = rect.x + (rect.width * 0.5F), .y = rect.y});
      made.add_line(Point{.x = rect.x, .y = rect.y + rect.height});
      made.close_subpath();
      return made;
    }

    friend auto operator==(const Wedge&, const Wedge&) -> bool = default;
  };

  class FixedWedge {
   public:
    auto path(Rect rect) const -> graphics::Path { return Wedge{}.path(rect); }

    auto layout_direction_behavior() const -> LayoutDirectionBehavior {
      return LayoutDirectionBehavior::fixed();
    }

    friend auto operator==(const FixedWedge&, const FixedWedge&)
        -> bool = default;
  };

  struct Shift {
    float x{0.0F};

    auto effect_value(Size) const -> graphics::ProjectionTransform {
      return graphics::ProjectionTransform{
          graphics::AffineTransform::translation(x, 0.0F)};
    }

    friend auto operator==(const Shift&, const Shift&) -> bool = default;
  };

  static auto behavior_name(LayoutDirectionBehavior behavior) -> const char* {
    return behavior.is_fixed() ? "fixed" : "mirrors";
  }
};

TEST_F(LayoutTypesTest, EnumerationsListEveryCaseInSwiftUIOrder) {
  EXPECT_EQ(all_axes, (std::array{Axis::horizontal, Axis::vertical}));
  EXPECT_EQ(theme::all_edges,
            (std::array{theme::Edge::top, theme::Edge::leading,
                        theme::Edge::bottom, theme::Edge::trailing}));
  EXPECT_EQ(all_horizontal_edges,
            (std::array{HorizontalEdge::leading, HorizontalEdge::trailing}));
  EXPECT_EQ(all_vertical_edges,
            (std::array{VerticalEdge::top, VerticalEdge::bottom}));
  EXPECT_EQ(all_layout_directions,
            (std::array{LayoutDirection::left_to_right,
                        LayoutDirection::right_to_left}));
  EXPECT_EQ(
      all_edge_corners,
      (std::array{EdgeCorner::top_leading, EdgeCorner::top_trailing,
                  EdgeCorner::bottom_leading, EdgeCorner::bottom_trailing}));
}

TEST_F(LayoutTypesTest, EdgeCornerSetsMatchSwiftUIRawValues) {
  EXPECT_EQ(EdgeCorners::none.raw_value(), 0U);
  EXPECT_EQ(EdgeCorners::leading.raw_value(), 5U);
  EXPECT_EQ(EdgeCorners::top.raw_value(), 3U);
  EXPECT_EQ(EdgeCorners::all.raw_value(), 15U);
  EXPECT_EQ(EdgeCorners::trailing,
            EdgeCorners::top_trailing | EdgeCorners::bottom_trailing);
  EXPECT_EQ(EdgeCorners::bottom,
            EdgeCorners::bottom_leading | EdgeCorners::bottom_trailing);
  EXPECT_EQ(EdgeCorners{EdgeCorner::bottom_leading},
            EdgeCorners::bottom_leading);
  EXPECT_TRUE(EdgeCorners::leading.contains(EdgeCorner::top_leading));
  EXPECT_TRUE(EdgeCorners::leading.contains(EdgeCorner::bottom_leading));
  EXPECT_FALSE(EdgeCorners::leading.contains(EdgeCorner::top_trailing));
  EXPECT_FALSE(EdgeCorners::none.contains(EdgeCorner::top_leading));
  EXPECT_TRUE(EdgeCorners::all.contains(EdgeCorners::top));
  EXPECT_TRUE(EdgeCorners::none.empty());
  EXPECT_EQ(EdgeCorners::top & EdgeCorners::leading, EdgeCorners::top_leading);
  for (const EdgeCorner corner : all_edge_corners) {
    EXPECT_TRUE(EdgeCorners::all.contains(corner));
  }
}

TEST_F(LayoutTypesTest, EdgeSetsBuildFromSingleEdges) {
  EXPECT_EQ(HorizontalEdges{HorizontalEdge::leading}, HorizontalEdges::leading);
  EXPECT_EQ(HorizontalEdges{HorizontalEdge::trailing},
            HorizontalEdges::trailing);
  EXPECT_EQ(HorizontalEdges::leading | HorizontalEdges::trailing,
            HorizontalEdges::all);
  EXPECT_TRUE(HorizontalEdges::all.contains(HorizontalEdges::trailing));
  EXPECT_FALSE(HorizontalEdges::leading.contains(HorizontalEdges::trailing));
  EXPECT_TRUE(HorizontalEdges{}.empty());
  EXPECT_EQ(VerticalEdges{VerticalEdge::top}, VerticalEdges::top);
  EXPECT_EQ(VerticalEdges{VerticalEdge::bottom}, VerticalEdges::bottom);
  EXPECT_TRUE(VerticalEdges::all.contains(VerticalEdge::bottom));
}

TEST_F(LayoutTypesTest, InterfaceOrientationsEnumerateAndDefaultToLandscape) {
  EXPECT_EQ(all_interface_orientations.size(), 4U);
  EXPECT_EQ(all_interface_orientations[0], InterfaceOrientation::portrait);
  EXPECT_EQ(all_interface_orientations[1],
            InterfaceOrientation::portrait_upside_down);
  EXPECT_EQ(all_interface_orientations[2],
            InterfaceOrientation::landscape_left);
  EXPECT_EQ(all_interface_orientations[3],
            InterfaceOrientation::landscape_right);
  EXPECT_NE(InterfaceOrientation::landscape_left,
            InterfaceOrientation::landscape_right);
  EXPECT_EQ(components::Environment::interface_orientation.fallback(),
            InterfaceOrientation::landscape_left);
}

TEST_F(LayoutTypesTest, AlignmentKeysAreStrictlyOrdered) {
  static constexpr AlignmentID first{
      "first", Axis::horizontal, [](const ViewDimensions&) { return 0.0F; }};
  static constexpr AlignmentID second{
      "second", Axis::horizontal, [](const ViewDimensions&) { return 0.0F; }};
  const std::array keys{AlignmentKey{HorizontalAlignment::leading},
                        AlignmentKey{HorizontalAlignment::trailing},
                        AlignmentKey{VerticalAlignment::top},
                        AlignmentKey{VerticalAlignment::last_text_baseline},
                        AlignmentKey{first},
                        AlignmentKey{second}};
  for (const AlignmentKey& left : keys) {
    EXPECT_FALSE(left < left);
    for (const AlignmentKey& right : keys) {
      if (left == right) {
        continue;
      }
      EXPECT_NE(left < right, right < left);
    }
  }
  EXPECT_TRUE(AlignmentKey{HorizontalAlignment::leading} <
              AlignmentKey{HorizontalAlignment::trailing});
  EXPECT_TRUE(AlignmentKey{HorizontalAlignment::trailing} <
              AlignmentKey{VerticalAlignment::top});
}

TEST_F(LayoutTypesTest, ScrollPhaseIsScrollingUnlessIdle) {
  EXPECT_FALSE(is_scrolling(ScrollPhase::idle));
  for (const ScrollPhase phase :
       {ScrollPhase::tracking, ScrollPhase::interacting,
        ScrollPhase::decelerating, ScrollPhase::animating}) {
    EXPECT_TRUE(is_scrolling(phase));
  }
}

TEST_F(LayoutTypesTest, ScrollPositionsCanNameOneAxis) {
  const ScrollPosition along{Axis::horizontal, 12.0F};
  EXPECT_EQ(along.x(), 12.0F);
  EXPECT_FALSE(along.y().has_value());
  EXPECT_FALSE(along.point().has_value());
  const ScrollPosition down{Axis::vertical, 34.0F};
  EXPECT_EQ(down.y(), 34.0F);
  EXPECT_FALSE(down.x().has_value());
  EXPECT_NE(along, down);
  const ScrollPosition both{Point{.x = 1.0F, .y = 2.0F}};
  EXPECT_EQ(both.x(), 1.0F);
  EXPECT_EQ(both.y(), 2.0F);
}

TEST_F(LayoutTypesTest, ScrollGeometryBoundsFollowTheOffsetAndContainer) {
  const ScrollGeometry built{
      .content_offset = Point{.x = 5.0F, .y = 7.0F},
      .content_size = Size{.width = 300.0F, .height = 500.0F},
      .content_insets =
          theme::Edges{
              .top = 4.0F, .leading = 3.0F, .bottom = 2.0F, .trailing = 1.0F},
      .container_size = Size{.width = 100.0F, .height = 200.0F}};
  EXPECT_EQ(built.bounds(),
            (Rect{.x = 5.0F, .y = 7.0F, .width = 100.0F, .height = 200.0F}));
  EXPECT_FLOAT_EQ(built.content_insets.leading, 3.0F);
  ScrollGeometry other = built;
  other.content_insets.top = 0.0F;
  EXPECT_NE(built, other);
}

TEST_F(LayoutTypesTest, ReservedRegionsFilterMoveAndMirrorIntoTheView) {
  const ReservedRegion occluded{
      ReservedRegion::ID{1U}, ReservedRegion::Kind::occlusion,
      Rect{.x = 100.0F, .y = 50.0F, .width = 40.0F, .height = 20.0F},
      theme::Edges{.leading = 2.0F, .trailing = 6.0F}};
  const ReservedRegion idle{
      ReservedRegion::ID{2U},
      ReservedRegion::Kind::occlusion,
      Rect{.x = 90.0F, .y = 40.0F, .width = 10.0F, .height = 10.0F},
      {},
      false};
  const ReservedRegion divided{
      ReservedRegion::ID{3U}, ReservedRegion::Kind::division,
      Rect{.x = 150.0F, .y = 0.0F, .width = 4.0F, .height = 200.0F}};
  const ReservedRegion away{
      ReservedRegion::ID{4U}, ReservedRegion::Kind::occlusion,
      Rect{.x = 900.0F, .y = 900.0F, .width = 4.0F, .height = 4.0F}};
  const auto regions = std::make_shared<const std::vector<ReservedRegion>>(
      std::vector{occluded, idle, divided, away});
  const Rect global{.x = 80.0F, .y = 30.0F, .width = 200.0F, .height = 100.0F};
  const GeometryProxy proxy{global, std::nullopt, nullptr, regions,
                            LayoutDirection::left_to_right};
  const std::vector<ReservedRegion> found =
      proxy.reserved_regions(ReservedRegion::Kind::occlusion);
  ASSERT_EQ(found.size(), 1U);
  EXPECT_EQ(found[0].id(), ReservedRegion::ID{1U});
  EXPECT_EQ(found[0].frame(),
            (Rect{.x = 20.0F, .y = 20.0F, .width = 40.0F, .height = 20.0F}));
  EXPECT_TRUE(found[0].is_active());
  EXPECT_EQ(
      proxy
          .reserved_regions(ReservedRegion::Kind::occlusion,
                            ReservedRegion::QueryOptions::include_inactive)
          .size(),
      2U);
  EXPECT_EQ(proxy.reserved_regions(ReservedRegion::Kind::division).size(), 1U);
  const GeometryProxy mirrored{global, std::nullopt, nullptr, regions,
                               LayoutDirection::right_to_left};
  const std::vector<ReservedRegion> flipped =
      mirrored.reserved_regions(ReservedRegion::Kind::occlusion);
  ASSERT_EQ(flipped.size(), 1U);
  EXPECT_FLOAT_EQ(flipped[0].frame().x, 140.0F);
  EXPECT_FLOAT_EQ(flipped[0].margins().leading, 6.0F);
  EXPECT_FLOAT_EQ(flipped[0].margins().trailing, 2.0F);
  const std::vector<ReservedRegion> kept = mirrored.reserved_regions(
      ReservedRegion::Kind::occlusion, {}, LayoutDirectionBehavior::fixed());
  ASSERT_EQ(kept.size(), 1U);
  EXPECT_FLOAT_EQ(kept[0].frame().x, 20.0F);
  EXPECT_EQ(kept[0].kind(), ReservedRegion::Kind::occlusion);
  EXPECT_TRUE(GeometryProxy{}
                  .reserved_regions(ReservedRegion::Kind::occlusion)
                  .empty());
  EXPECT_NE(proxy, mirrored);
}

TEST_F(LayoutTypesTest, ShapesDeclareTheirLayoutDirectionBehaviorLikeSwiftUI) {
  EXPECT_STREQ(
      behavior_name(graphics::AnyShape{Wedge{}}.layout_direction_behavior()),
      "mirrors");
  EXPECT_STREQ(behavior_name(graphics::AnyShape{
                   FixedWedge{}}.layout_direction_behavior()),
               "fixed");
  EXPECT_STREQ(
      behavior_name(
          graphics::AnyShape{graphics::Path{}}.layout_direction_behavior()),
      "mirrors");
  EXPECT_FALSE(theme::Rectangle{}.mirrors());
  EXPECT_FALSE(theme::RoundedRectangle{4.0F}.mirrors());
  EXPECT_FALSE(theme::Capsule{}.mirrors());
  EXPECT_FALSE(theme::Circle{}.mirrors());
  EXPECT_FALSE(theme::Ellipse{}.mirrors());
  EXPECT_TRUE(theme::UnevenRoundedRectangle(4.0F, 0.0F, 0.0F, 0.0F).mirrors());
  EXPECT_STREQ(
      behavior_name(
          graphics::AnyShape{theme::Rectangle{}}.layout_direction_behavior()),
      "fixed");
  EXPECT_STREQ(behavior_name(graphics::OffsetShape{theme::Rectangle{},
                                                   Size{.width = 1.0F}}
                                 .layout_direction_behavior()),
               "fixed");
  EXPECT_STREQ(behavior_name(graphics::OffsetShape{Wedge{}, Size{.width = 1.0F}}
                                 .layout_direction_behavior()),
               "mirrors");
  EXPECT_STREQ(behavior_name(graphics::RotatedShape{
                   theme::Rectangle{}, motion::Angle::degrees(10.0)}
                                 .layout_direction_behavior()),
               "fixed");
  EXPECT_STREQ(behavior_name(graphics::ScaledShape{
                   theme::Circle{}, Size{.width = 2.0F, .height = 2.0F}}
                                 .layout_direction_behavior()),
               "fixed");
  EXPECT_STREQ(behavior_name(graphics::TransformedShape{
                   theme::Capsule{}, graphics::AffineTransform{}}
                                 .layout_direction_behavior()),
               "fixed");
  EXPECT_STREQ(behavior_name(graphics::TransformedShape{
                   Wedge{}, graphics::AffineTransform{}}
                                 .layout_direction_behavior()),
               "mirrors");
  EXPECT_STREQ(
      behavior_name(graphics::AnyShape{
          graphics::RotatedShape{FixedWedge{}, motion::Angle::degrees(5.0)}}
                        .layout_direction_behavior()),
      "fixed");
}

TEST_F(LayoutTypesTest, IgnoredByLayoutWrapsAGeometryEffect) {
  const IgnoredByLayoutEffect ignored{Shift{.x = 30.0F}};
  static_assert(GeometryEffect<IgnoredByLayoutEffect<Shift>>);
  static_assert(IgnoredByLayoutEffect<Shift>::ignored_by_layout);
  EXPECT_EQ(ignored.effect(), Shift{.x = 30.0F});
  EXPECT_FLOAT_EQ(ignored.effect_value(Size{}).affine().tx(), 30.0F);
  EXPECT_EQ(ignored, IgnoredByLayoutEffect{Shift{.x = 30.0F}});
}

TEST_F(LayoutTypesTest, DrawingOptionsAreAnOptionSet) {
  using Options = text::TextLayout::DrawingOptions;
  EXPECT_EQ(Options::disables_subpixel_quantization.raw_value(), 1U);
  EXPECT_FALSE(Options{}.contains(Options::disables_subpixel_quantization));
  EXPECT_TRUE((Options{} | Options::disables_subpixel_quantization)
                  .contains(Options::disables_subpixel_quantization));
}

TEST_F(LayoutTypesTest, ContainersReadTheirContentBack) {
  int built = 0;
  const components::GeometryReader reader{[&built](const GeometryProxy&) {
    ++built;
    return components::Text("inside");
  }};
  ASSERT_EQ(reader.content().type(),
            ElementType::of<components::GeometryReader::Content>());
  (void)static_cast<const components::GeometryReader::Content&>(
      reader.content())
      .content()(GeometryProxy{});
  EXPECT_EQ(built, 1);
  const components::ScrollViewReader scroller{
      [](ScrollViewProxy) { return components::Text("inside"); }};
  ASSERT_EQ(scroller.content().type(),
            ElementType::of<components::ScrollViewReader::Content>());
  EXPECT_TRUE(static_cast<bool>(
      static_cast<const components::ScrollViewReader::Content&>(
          scroller.content())
          .content()));
  const components::ScrollView scroll{components::Text("only").id("only")};
  ASSERT_TRUE(scroll.content_view().key().has_value());
  EXPECT_EQ(*scroll.content_view().key(), ElementKey{"only"});
  const components::Spacer spacer{24.0F};
  ASSERT_EQ(spacer.content().type(),
            ElementType::of<components::Spacer::Content>());
  EXPECT_FLOAT_EQ(
      static_cast<const components::Spacer::Content&>(spacer.content())
          .minimum_length(),
      24.0F);
}

TEST_F(LayoutTypesTest, ListRowSeparatorGuidesAreHorizontalAlignments) {
  EXPECT_EQ(list_row_separator_leading.axis(), Axis::horizontal);
  EXPECT_EQ(list_row_separator_trailing.axis(), Axis::horizontal);
  EXPECT_FLOAT_EQ(
      list_row_separator_trailing.default_value(ViewDimensions{40.0F, 10.0F}),
      40.0F);
  EXPECT_FLOAT_EQ(
      list_row_separator_leading.default_value(ViewDimensions{40.0F, 10.0F}),
      0.0F);
}

TEST_F(LayoutTypesTest, EdgeInsetsGrowByTheCornerInsetsOnTheChosenEdges) {
  const theme::Edges insets{
      .top = 1.0F, .leading = 2.0F, .bottom = 3.0F, .trailing = 4.0F};
  const theme::RectangleCornerInsets corners{
      .top_leading = {.width = 10.0F, .height = 20.0F},
      .top_trailing = {.width = 30.0F, .height = 40.0F},
      .bottom_leading = {.width = 50.0F, .height = 60.0F},
      .bottom_trailing = {.width = 70.0F, .height = 80.0F}};
  EXPECT_EQ(
      insets.inset(corners),
      (theme::Edges{
          .top = 41.0F, .leading = 52.0F, .bottom = 83.0F, .trailing = 74.0F}));
  EXPECT_EQ(
      insets.inset(corners, theme::Edge::top),
      (theme::Edges{
          .top = 41.0F, .leading = 2.0F, .bottom = 3.0F, .trailing = 4.0F}));
  EXPECT_EQ(
      insets.inset(corners, theme::Edge::trailing),
      (theme::Edges{
          .top = 1.0F, .leading = 2.0F, .bottom = 3.0F, .trailing = 74.0F}));
  EXPECT_EQ(theme::RectangleCornerInsets{}.top_leading, theme::CornerSize{});
}

TEST_F(LayoutTypesTest, SafeAreaRegionsAndScenePaddingMatchSwiftUI) {
  using composition::SafeAreaRegions;
  EXPECT_EQ(SafeAreaRegions::container.raw_value(), 1U);
  EXPECT_EQ(SafeAreaRegions::keyboard.raw_value(), 2U);
  EXPECT_EQ(SafeAreaRegions::all.raw_value(), 0xFFU);
  EXPECT_TRUE(SafeAreaRegions::all.contains(SafeAreaRegions::container |
                                            SafeAreaRegions::keyboard));
  EXPECT_TRUE(SafeAreaRegions::all.contains(SafeAreaRegions::keyboard));
  EXPECT_FALSE(SafeAreaRegions::keyboard.contains(SafeAreaRegions::container));
  EXPECT_EQ(composition::ScenePadding::minimum,
            composition::ScenePadding::minimum);
  EXPECT_EQ(composition::ScenePadding::minimum.length(), theme::Space{20.0F});
}

TEST_F(LayoutTypesTest, ContainerCornersMirrorForRightToLeft) {
  const composition::ContainerCorners corners{
      composition::Rect{.width = 100.0F, .height = 100.0F},
      theme::RectangleCornerRadii{.top_leading = 20.0F},
      composition::LayoutDirection::right_to_left};
  const composition::Rect view{
      .x = 56.0F, .y = 4.0F, .width = 40.0F, .height = 40.0F};
  EXPECT_NEAR(corners.insets(view).top_leading.width, 16.0F, 0.01F);
  EXPECT_NEAR(corners.concentric_radii(view).top_leading, 16.0F, 0.01F);
  EXPECT_NEAR(corners
                  .insets(composition::Rect{
                      .x = 4.0F, .y = 4.0F, .width = 40.0F, .height = 40.0F})
                  .top_leading.width,
              0.0F, 0.01F);
}

}
