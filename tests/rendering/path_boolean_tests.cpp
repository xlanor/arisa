#include <arisa/components/button_border_shape.hpp>
#include <arisa/composition/point.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/graphics/any_shape.hpp>
#include <arisa/graphics/combined_shape.hpp>
#include <arisa/graphics/offset_shape.hpp>
#include <arisa/graphics/path.hpp>
#include <arisa/graphics/projection_transform.hpp>
#include <arisa/graphics/rotated_shape.hpp>
#include <arisa/graphics/scaled_shape.hpp>
#include <arisa/graphics/shape_role.hpp>
#include <arisa/graphics/sized_shape.hpp>
#include <arisa/graphics/stroke_style.hpp>
#include <arisa/graphics/transformed_shape.hpp>
#include <arisa/motion/angle.hpp>
#include <arisa/motion/unit_point.hpp>
#include <arisa/theme/shape.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <variant>
#include <vector>

namespace arisa::graphics {

class PathBooleanTest : public ::testing::Test {
 protected:
  static auto square(float x, float y, float side) -> Path {
    return Path{
        composition::Rect{.x = x, .y = y, .width = side, .height = side}};
  }

  static auto area(const Path& path, bool eo_fill = false) -> float {
    int inside = 0;
    for (int y = 0; y < 200; ++y) {
      for (int x = 0; x < 200; ++x) {
        if (path.contains(
                composition::Point{.x = (static_cast<float>(x) * 0.5F) + 0.25F,
                                   .y = (static_cast<float>(y) * 0.5F) + 0.25F},
                eo_fill)) {
          ++inside;
        }
      }
    }
    return static_cast<float>(inside) * 0.25F;
  }

  static auto length(const Path& path) -> float {
    float total = 0.0F;
    composition::Point at{};
    for (const Path::Element& element : path.elements()) {
      if (const auto* moved = std::get_if<Path::Move>(&element)) {
        at = moved->to;
      } else if (const auto* line = std::get_if<Path::Line>(&element)) {
        total += std::hypot(line->to.x - at.x, line->to.y - at.y);
        at = line->to;
      }
    }
    return total;
  }

  static auto at(composition::Point point) -> composition::Point {
    return point;
  }
};

TEST_F(PathBooleanTest, CombinesOverlappingSquares) {
  const Path a = square(10.0F, 10.0F, 40.0F);
  const Path b = square(30.0F, 30.0F, 40.0F);
  EXPECT_NEAR(area(a.union_with(b)), 2800.0F, 20.0F);
  EXPECT_NEAR(area(a.intersection(b)), 400.0F, 10.0F);
  EXPECT_NEAR(area(a.subtracting(b)), 1200.0F, 15.0F);
  EXPECT_NEAR(area(a.symmetric_difference(b)), 2400.0F, 20.0F);
  EXPECT_TRUE(a.subtracting(b).contains({.x = 15.0F, .y = 15.0F}));
  EXPECT_FALSE(a.subtracting(b).contains({.x = 40.0F, .y = 40.0F}));
  EXPECT_FALSE(a.symmetric_difference(b).contains({.x = 40.0F, .y = 40.0F}));
  EXPECT_TRUE(a.symmetric_difference(b).contains({.x = 65.0F, .y = 65.0F}));
}

TEST_F(PathBooleanTest, HandlesDegenerateInput) {
  const Path empty;
  const Path a = square(10.0F, 10.0F, 40.0F);
  EXPECT_TRUE(empty.union_with(empty).is_empty());
  EXPECT_NEAR(area(a.union_with(empty)), 1600.0F, 15.0F);
  EXPECT_TRUE(a.intersection(empty).is_empty());
  EXPECT_TRUE(empty.subtracting(a).is_empty());
  EXPECT_NEAR(area(a.union_with(a)), 1600.0F, 15.0F);
  EXPECT_NEAR(area(a.intersection(a)), 1600.0F, 15.0F);
  EXPECT_TRUE(a.subtracting(a).is_empty());
  const Path touching = square(50.0F, 10.0F, 40.0F);
  EXPECT_NEAR(area(a.union_with(touching)), 3200.0F, 25.0F);
  EXPECT_LT(area(a.intersection(touching)), 1.0F);
  Path flat;
  flat.move({.x = 0.0F, .y = 20.0F});
  flat.add_line({.x = 80.0F, .y = 20.0F});
  flat.add_line({.x = 40.0F, .y = 20.0F});
  flat.close_subpath();
  EXPECT_NEAR(area(a.union_with(flat)), 1600.0F, 15.0F);
  Path bow;
  bow.move({.x = 0.0F, .y = 0.0F});
  bow.add_line({.x = 40.0F, .y = 40.0F});
  bow.add_line({.x = 40.0F, .y = 0.0F});
  bow.add_line({.x = 0.0F, .y = 40.0F});
  bow.close_subpath();
  EXPECT_NEAR(area(bow.normalized(false)), area(bow), 10.0F);
}

TEST_F(PathBooleanTest, FollowsTheEvenOddRule) {
  Path ring = square(10.0F, 10.0F, 60.0F);
  ring.add_path(square(30.0F, 30.0F, 20.0F));
  EXPECT_NEAR(area(ring.normalized(true)), 3200.0F, 25.0F);
  EXPECT_NEAR(area(ring.normalized(false)), 3600.0F, 25.0F);
  EXPECT_FALSE(ring.normalized(true).contains({.x = 40.0F, .y = 40.0F}));
  const Path block = square(35.0F, 35.0F, 10.0F);
  EXPECT_TRUE(ring.intersection(block, true).is_empty());
  EXPECT_NEAR(area(ring.intersection(block, false)), 100.0F, 5.0F);
}

TEST_F(PathBooleanTest, ClipsLinesAgainstARegion) {
  Path line;
  line.move({.x = 0.0F, .y = 50.0F});
  line.add_line({.x = 100.0F, .y = 50.0F});
  const Path region = square(20.0F, 20.0F, 40.0F);
  EXPECT_NEAR(length(line.line_intersection(region)), 40.0F, 0.01F);
  EXPECT_NEAR(length(line.line_subtraction(region)), 60.0F, 0.01F);
  Path missing;
  missing.move({.x = 0.0F, .y = 90.0F});
  missing.add_line({.x = 100.0F, .y = 90.0F});
  EXPECT_TRUE(missing.line_intersection(region).is_empty());
  EXPECT_NEAR(length(missing.line_subtraction(region)), 100.0F, 0.01F);
}

TEST_F(PathBooleanTest, StrokedPathOutlinesTheStroke) {
  Path line;
  line.move({.x = 10.0F, .y = 50.0F});
  line.add_line({.x = 90.0F, .y = 50.0F});
  const Path butt = line.stroked_path(StrokeStyle{.line_width = 10.0F});
  EXPECT_NEAR(area(butt), 800.0F, 15.0F);
  EXPECT_TRUE(butt.contains({.x = 50.0F, .y = 54.0F}));
  EXPECT_FALSE(butt.contains({.x = 50.0F, .y = 56.0F}));
  EXPECT_FALSE(butt.contains({.x = 8.0F, .y = 50.0F}));
  const Path capped = line.stroked_path(
      StrokeStyle{.line_width = 10.0F, .line_cap = LineCap::square});
  EXPECT_TRUE(capped.contains({.x = 8.0F, .y = 50.0F}));
  const Path rounded = line.stroked_path(
      StrokeStyle{.line_width = 10.0F, .line_cap = LineCap::round});
  EXPECT_TRUE(rounded.contains({.x = 7.0F, .y = 50.0F}));
  EXPECT_FALSE(rounded.contains({.x = 6.0F, .y = 46.0F}));
  const Path dashed = line.stroked_path(
      StrokeStyle{.line_width = 10.0F, .dash = {10.0F, 10.0F}});
  EXPECT_NEAR(area(dashed), 400.0F, 15.0F);
  EXPECT_TRUE(square(10.0F, 10.0F, 0.0F)
                  .stroked_path(StrokeStyle{.line_width = 0.0F})
                  .is_empty());
}

TEST_F(PathBooleanTest, TrimmedPathKeepsCurvesAndFractions) {
  Path line;
  line.move({.x = 0.0F, .y = 0.0F});
  line.add_line({.x = 100.0F, .y = 0.0F});
  const Path half = line.trimmed_path(0.25, 0.75);
  EXPECT_NEAR(length(half), 50.0F, 0.01F);
  ASSERT_TRUE(half.current_point().has_value());
  EXPECT_NEAR(half.current_point()->x, 75.0F, 0.01F);
  EXPECT_TRUE(line.trimmed_path(0.6, 0.4).is_empty());
  const Path circle = Path::ellipse_in(composition::Rect{
      .x = 0.0F, .y = 0.0F, .width = 100.0F, .height = 100.0F});
  const Path arc = circle.trimmed_path(0.0, 0.5);
  EXPECT_TRUE(
      std::ranges::any_of(arc.elements(), [](const Path::Element& each) {
        return std::holds_alternative<Path::Curve>(each);
      }));
  ASSERT_TRUE(arc.current_point().has_value());
  EXPECT_NEAR(arc.current_point()->x, 0.0F, 1.5F);
  EXPECT_NEAR(arc.current_point()->y, 50.0F, 1.5F);
}

TEST_F(PathBooleanTest, AddsTangentRelativeArcsAndRects) {
  Path corner;
  corner.move({.x = 0.0F, .y = 0.0F});
  corner.add_arc(composition::Point{.x = 100.0F, .y = 0.0F},
                 composition::Point{.x = 100.0F, .y = 100.0F}, 20.0F);
  ASSERT_TRUE(corner.current_point().has_value());
  EXPECT_NEAR(corner.current_point()->x, 100.0F, 0.01F);
  EXPECT_NEAR(corner.current_point()->y, 20.0F, 0.01F);
  Path relative;
  relative.add_relative_arc({.x = 50.0F, .y = 50.0F}, 10.0F,
                            motion::Angle::degrees(0.0),
                            motion::Angle::degrees(90.0));
  ASSERT_TRUE(relative.current_point().has_value());
  EXPECT_NEAR(relative.current_point()->x, 50.0F, 0.01F);
  EXPECT_NEAR(relative.current_point()->y, 60.0F, 0.01F);
  Path rects;
  const std::array<composition::Rect, 2> boxes{
      composition::Rect{.x = 0.0F, .y = 0.0F, .width = 10.0F, .height = 10.0F},
      composition::Rect{
          .x = 20.0F, .y = 0.0F, .width = 10.0F, .height = 10.0F}};
  rects.add_rects(boxes, AffineTransform::translation(5.0F, 5.0F));
  EXPECT_TRUE(rects.contains({.x = 6.0F, .y = 6.0F}));
  EXPECT_TRUE(rects.contains({.x = 26.0F, .y = 6.0F}));
  EXPECT_FALSE(rects.contains({.x = 18.0F, .y = 6.0F}));
}

TEST_F(PathBooleanTest, ProjectionTransformsInvert) {
  const ProjectionTransform forward{std::array<float, 9>{
      2.0F, 0.0F, 0.001F, 0.0F, 3.0F, 0.0F, 5.0F, 7.0F, 1.0F}};
  ProjectionTransform back = forward;
  ASSERT_TRUE(back.invert());
  const composition::Point point{.x = 12.0F, .y = 9.0F};
  const std::optional<composition::Point> moved = forward.apply(point);
  ASSERT_TRUE(moved.has_value());
  const std::optional<composition::Point> returned = back.apply(*moved);
  ASSERT_TRUE(returned.has_value());
  EXPECT_NEAR(returned->x, point.x, 1.0e-3F);
  EXPECT_NEAR(returned->y, point.y, 1.0e-3F);
  EXPECT_EQ(forward.inverted(), back);
  ProjectionTransform singular{std::array<float, 9>{
      0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F}};
  EXPECT_FALSE(singular.invert());
  EXPECT_EQ(singular.inverted(), singular);
  EXPECT_LT(motion::Angle::degrees(10.0), motion::Angle::degrees(20.0));
  EXPECT_GT(motion::Angle::radians(1.0), motion::Angle::degrees(10.0));
}

TEST_F(PathBooleanTest, ShapesCombineSizeAndKeepTheirRole) {
  const AnyShape circle{theme::Circle()};
  const AnyShape box{theme::Rectangle()};
  const composition::Rect frame{
      .x = 0.0F, .y = 0.0F, .width = 100.0F, .height = 100.0F};
  EXPECT_FALSE(
      box.subtracting(circle).path(frame).contains({.x = 50.0F, .y = 50.0F}));
  EXPECT_TRUE(
      box.subtracting(circle).path(frame).contains({.x = 2.0F, .y = 2.0F}));
  EXPECT_TRUE(
      box.intersection(circle).path(frame).contains({.x = 50.0F, .y = 50.0F}));
  EXPECT_FALSE(
      box.intersection(circle).path(frame).contains({.x = 2.0F, .y = 2.0F}));
  EXPECT_TRUE(
      circle.union_with(box).path(frame).contains({.x = 2.0F, .y = 2.0F}));
  EXPECT_FALSE(box.symmetric_difference(circle).path(frame).contains(
      {.x = 50.0F, .y = 50.0F}));
  EXPECT_FALSE(circle.line_intersection(box).path(frame).is_empty());
  EXPECT_TRUE(box.line_intersection(circle).path(frame).is_empty());
  EXPECT_FALSE(box.line_subtraction(circle).path(frame).is_empty());
  const Path sized = box.size(20.0F, 10.0F).path(frame);
  EXPECT_EQ(sized.bounding_rect(),
            (composition::Rect{
                .x = 0.0F, .y = 0.0F, .width = 20.0F, .height = 10.0F}));
  const Path centred =
      box.size({.width = 20.0F, .height = 10.0F}, motion::UnitPoint::center)
          .path(frame);
  EXPECT_EQ(centred.bounding_rect(),
            (composition::Rect{
                .x = 40.0F, .y = 45.0F, .width = 20.0F, .height = 10.0F}));
  EXPECT_EQ(box.size(composition::Size{.width = 5.0F, .height = 5.0F})
                .path(frame)
                .bounding_rect()
                .width,
            5.0F);
  EXPECT_EQ(box.size(5.0F, 5.0F, motion::UnitPoint::bottom_trailing)
                .path(frame)
                .bounding_rect()
                .x,
            95.0F);
  EXPECT_EQ(box.role(), ShapeRole::fill);
  EXPECT_EQ(box.offset(2.0F, 2.0F).role(), ShapeRole::fill);
  EXPECT_EQ(OffsetShape(box, {}).role(), ShapeRole::fill);
  EXPECT_EQ(RotatedShape(box, {}).role(), ShapeRole::fill);
  EXPECT_EQ(ScaledShape(box, {.width = 1.0F, .height = 1.0F}).role(),
            ShapeRole::fill);
  EXPECT_EQ(TransformedShape(box, AffineTransform{}).role(), ShapeRole::fill);
  const OffsetShape moved{box, {.width = 10.0F, .height = 0.0F}};
  EXPECT_EQ(moved.inset(5.0F).path(frame).bounding_rect(),
            (composition::Rect{
                .x = 15.0F, .y = 5.0F, .width = 90.0F, .height = 90.0F}));
  const RotatedShape turned{box, motion::Angle::degrees(0.0)};
  EXPECT_EQ(turned.inset(5.0F).path(frame).bounding_rect().x, 5.0F);
}

TEST_F(PathBooleanTest, CircleCentresTheShorterSide) {
  const Path circle = AnyShape{theme::Circle()}.path(composition::Rect{
      .x = 0.0F, .y = 0.0F, .width = 100.0F, .height = 40.0F});
  const composition::Rect bounds = circle.bounding_rect();
  EXPECT_NEAR(bounds.x, 30.0F, 0.01F);
  EXPECT_NEAR(bounds.width, 40.0F, 0.01F);
  EXPECT_TRUE(circle.contains({.x = 50.0F, .y = 20.0F}));
  EXPECT_FALSE(circle.contains({.x = 25.0F, .y = 20.0F}));
  EXPECT_FALSE(circle.contains({.x = 75.0F, .y = 20.0F}));
  EXPECT_EQ(theme::Shape::circle().form, theme::ShapeForm::circle);
  EXPECT_FALSE(theme::Circle().mirrors());
}

TEST_F(PathBooleanTest, RoundedShapesExposeTheirCorners) {
  using theme::ConcentricCorner;
  using theme::RoundedRectangularShapeCorners;
  const composition::Size frame{.width = 100.0F, .height = 40.0F};
  EXPECT_EQ(theme::Rectangle().corners(frame),
            RoundedRectangularShapeCorners::fixed(0.0F));
  EXPECT_EQ(theme::RoundedRectangle(8.0F).corners(std::nullopt),
            RoundedRectangularShapeCorners::fixed(8.0F));
  EXPECT_EQ(theme::Capsule().corners(frame),
            RoundedRectangularShapeCorners::fixed(20.0F));
  EXPECT_FALSE(theme::Capsule().corners(std::nullopt).has_value());
  EXPECT_EQ(theme::Circle().corners(frame),
            RoundedRectangularShapeCorners::fixed(20.0F));
  EXPECT_FALSE(theme::Ellipse().corners(frame).has_value());
  const auto uneven =
      theme::UnevenRoundedRectangle(1.0F, 2.0F, 3.0F, 4.0F).corners(frame);
  ASSERT_TRUE(uneven.has_value());
  EXPECT_EQ(uneven->top_leading, ConcentricCorner::fixed(1.0F));
  EXPECT_EQ(uneven->bottom_leading, ConcentricCorner::fixed(2.0F));
  EXPECT_EQ(uneven->bottom_trailing, ConcentricCorner::fixed(3.0F));
  EXPECT_EQ(uneven->top_trailing, ConcentricCorner::fixed(4.0F));
  EXPECT_EQ((*uneven)[RoundedRectangularShapeCorners::Corner::top_trailing],
            ConcentricCorner::fixed(4.0F));
  EXPECT_EQ(RoundedRectangularShapeCorners::concentric(2.0F).top_leading,
            ConcentricCorner::concentric(2.0F));
  EXPECT_EQ(RoundedRectangularShapeCorners::all(ConcentricCorner::fixed(3.0F))
                .bottom_trailing,
            ConcentricCorner::fixed(3.0F));
  EXPECT_EQ(AnyShape{theme::Capsule()}.corners(frame),
            RoundedRectangularShapeCorners::fixed(20.0F));
  struct Pill {
    auto path(composition::Rect rect) const -> Path {
      return Path{rect, std::min(rect.width, rect.height) * 0.5F};
    }

    auto corners(std::optional<composition::Size>) const
        -> std::optional<RoundedRectangularShapeCorners> {
      return RoundedRectangularShapeCorners::fixed(6.0F);
    }
  };
  static_assert(RoundedRectangularShape<Pill>);
  EXPECT_EQ(AnyShape{Pill{}}.corners(frame),
            RoundedRectangularShapeCorners::fixed(6.0F));
}

TEST_F(PathBooleanTest, CornerStylesAndPairsBuildTheirShapes) {
  using theme::ConcentricCorner;
  EXPECT_EQ(theme::Capsule(theme::RoundedCornerStyle::continuous).style(),
            theme::RoundedCornerStyle::continuous);
  EXPECT_EQ(theme::Capsule().style(), theme::RoundedCornerStyle::circular);
  EXPECT_TRUE(
      theme::Shape::capsule(theme::RoundedCornerStyle::continuous).continuous);
  const theme::RoundedRectangle elliptical{
      composition::Size{.width = 20.0F, .height = 10.0F}};
  EXPECT_EQ(elliptical.corner_size(),
            (composition::Size{.width = 20.0F, .height = 10.0F}));
  EXPECT_TRUE(elliptical.elliptical());
  EXPECT_EQ(theme::RoundedRectangle(6.0F).corner_size(),
            (composition::Size{.width = 6.0F, .height = 6.0F}));
  const Path rounded = AnyShape{elliptical}.path(composition::Rect{
      .x = 0.0F, .y = 0.0F, .width = 100.0F, .height = 60.0F});
  EXPECT_FALSE(rounded.contains({.x = 1.0F, .y = 2.0F}));
  EXPECT_TRUE(rounded.contains({.x = 12.0F, .y = 2.0F}));
  EXPECT_EQ(theme::Shape::rect(composition::Size{.width = 4.0F, .height = 2.0F})
                .corner_extent,
            (composition::Size{.width = 4.0F, .height = 2.0F}));
  const ConcentricCorner fixed = ConcentricCorner::fixed(5.0F);
  const ConcentricCorner rounded_corner = ConcentricCorner::concentric(1.0F);
  const theme::ConcentricRectangle pair{
      theme::UniformTopCorners{fixed},
      theme::UniformBottomCorners{rounded_corner}};
  EXPECT_EQ(pair.concentric[0], fixed);
  EXPECT_EQ(pair.concentric[3], fixed);
  EXPECT_EQ(pair.concentric[1], rounded_corner);
  EXPECT_EQ(pair.uniform_edges,
            theme::Shape::uniform_top | theme::Shape::uniform_bottom);
  EXPECT_EQ(
      theme::ConcentricRectangle(theme::UniformLeadingCorners{fixed},
                                 theme::UniformTrailingCorners{rounded_corner})
          .uniform_edges,
      theme::Shape::uniform_leading | theme::Shape::uniform_trailing);
  EXPECT_EQ(theme::ConcentricRectangle(theme::UniformTopCorners{fixed},
                                       rounded_corner, rounded_corner)
                .uniform_edges,
            theme::Shape::uniform_top);
  EXPECT_EQ(theme::ConcentricRectangle(theme::UniformBottomCorners{fixed},
                                       rounded_corner, rounded_corner)
                .uniform_edges,
            theme::Shape::uniform_bottom);
  EXPECT_EQ(theme::ConcentricRectangle(theme::UniformLeadingCorners{fixed},
                                       rounded_corner, rounded_corner)
                .uniform_edges,
            theme::Shape::uniform_leading);
  EXPECT_EQ(theme::ConcentricRectangle(theme::UniformTrailingCorners{fixed},
                                       rounded_corner, rounded_corner)
                .uniform_edges,
            theme::Shape::uniform_trailing);
  EXPECT_EQ(theme::Shape::rect(theme::UniformTopCorners{fixed},
                               theme::UniformBottomCorners{fixed}),
            static_cast<const theme::Shape&>(theme::ConcentricRectangle(
                theme::UniformTopCorners{fixed},
                theme::UniformBottomCorners{fixed})));
  EXPECT_EQ(theme::Shape::rect(theme::UniformLeadingCorners{fixed},
                               theme::UniformTrailingCorners{fixed})
                .uniform_edges,
            theme::Shape::uniform_leading | theme::Shape::uniform_trailing);
  EXPECT_EQ(theme::Shape::rect(theme::UniformTopCorners{fixed}, fixed, fixed)
                .uniform_edges,
            theme::Shape::uniform_top);
  EXPECT_EQ(theme::Shape::rect(theme::UniformBottomCorners{fixed}, fixed, fixed)
                .uniform_edges,
            theme::Shape::uniform_bottom);
  EXPECT_EQ(
      theme::Shape::rect(theme::UniformLeadingCorners{fixed}, fixed, fixed)
          .uniform_edges,
      theme::Shape::uniform_leading);
  EXPECT_EQ(
      theme::Shape::rect(theme::UniformTrailingCorners{fixed}, fixed, fixed)
          .uniform_edges,
      theme::Shape::uniform_trailing);
}

TEST_F(PathBooleanTest, ButtonBorderShapesInsetAndProducePaths) {
  const components::ButtonBorderShape capsule =
      components::ButtonBorderShape::capsule();
  const composition::Rect frame{
      .x = 0.0F, .y = 0.0F, .width = 100.0F, .height = 40.0F};
  EXPECT_FALSE(capsule.path(frame).contains({.x = 1.0F, .y = 1.0F}));
  EXPECT_TRUE(capsule.path(frame).contains({.x = 50.0F, .y = 1.0F}));
  const components::ButtonBorderShape inset = capsule.inset(5.0F);
  EXPECT_EQ(inset.inset_amount(), 5.0F);
  EXPECT_FALSE(inset.path(frame).contains({.x = 50.0F, .y = 2.0F}));
  EXPECT_TRUE(components::ButtonBorderShape::rounded_rectangle(4.0F)
                  .path(frame)
                  .contains({.x = 2.0F, .y = 2.0F}));
}

TEST_F(PathBooleanTest, ContainsFollowsTheFillRule) {
  graphics::Path ring{
      composition::Rect{.x = 0.0F, .y = 0.0F, .width = 40.0F, .height = 40.0F}};
  ring.add_rect(composition::Rect{
      .x = 10.0F, .y = 10.0F, .width = 20.0F, .height = 20.0F});
  EXPECT_TRUE(ring.contains({.x = 20.0F, .y = 20.0F}));
  EXPECT_FALSE(ring.contains({.x = 20.0F, .y = 20.0F}, true));
  EXPECT_TRUE(ring.contains({.x = 5.0F, .y = 5.0F}, true));
  EXPECT_FALSE(ring.contains({.x = 50.0F, .y = 5.0F}));
}

}
