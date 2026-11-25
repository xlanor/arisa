#include <layout_fixture.hpp>

#include <bridge/nanovg/path_tessellator.hpp>

#include <arisa/components/mesh_gradient.hpp>
#include <arisa/components/shape_view.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/point.hpp>
#include <arisa/graphics/any_shape.hpp>
#include <arisa/graphics/gradient.hpp>
#include <arisa/graphics/offset_shape.hpp>
#include <arisa/graphics/path.hpp>
#include <arisa/graphics/rotated_shape.hpp>
#include <arisa/graphics/scaled_shape.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/graphics/stroke_style.hpp>
#include <arisa/graphics/style_hierarchy.hpp>
#include <arisa/graphics/transformed_shape.hpp>
#include <arisa/motion/angle.hpp>
#include <arisa/provider/headless/null_gpu.hpp>
#include <arisa/rendering/painter.hpp>
#include <arisa/rendering/shape_painter.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/shape.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <map>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

namespace arisa::rendering {

using Tessellator = bridge::nanovg::PathTessellator;

class PathTest : public ::testing::Test, protected layout::LayoutFixture {
 protected:
  struct Sample {
    float coverage{0.0F};
    std::array<float, 4> colour{};
  };

  static auto tessellator() -> Tessellator {
    auto made = Tessellator::create();
    EXPECT_TRUE(made.has_value());
    return std::move(*made);
  }

  static auto cross(std::array<float, 2> a, std::array<float, 2> b,
                    std::array<float, 2> c) -> float {
    return ((b[0] - a[0]) * (c[1] - a[1])) - ((b[1] - a[1]) * (c[0] - a[0]));
  }

  template <class Vertex, class Read>
  static auto probe(std::span<const Vertex> vertices,
                    std::span<const std::uint32_t> indices, float x, float y,
                    Read read) -> std::optional<std::array<float, 4>> {
    std::optional<std::array<float, 4>> found;
    for (std::size_t index = 0; index + 2 < indices.size(); index += 3) {
      const auto [ax, ay, a] = read(vertices[indices[index]]);
      const auto [bx, by, b] = read(vertices[indices[index + 1]]);
      const auto [cx, cy, c] = read(vertices[indices[index + 2]]);
      const float area = cross({ax, ay}, {bx, by}, {cx, cy});
      if (std::abs(area) < 1.0e-6F) {
        continue;
      }
      const float wa = cross({bx, by}, {cx, cy}, {x, y}) / area;
      const float wb = cross({cx, cy}, {ax, ay}, {x, y}) / area;
      const float wc = 1.0F - wa - wb;
      if (wa < -1.0e-4F || wb < -1.0e-4F || wc < -1.0e-4F) {
        continue;
      }
      std::array<float, 4> mixed{};
      for (std::size_t channel = 0; channel < 4; ++channel) {
        mixed[channel] =
            (wa * a[channel]) + (wb * b[channel]) + (wc * c[channel]);
      }
      if (!found.has_value() || mixed[3] > (*found)[3]) {
        found = mixed;
      }
    }
    return found;
  }

  static auto coverage(const Tessellator::Mesh& mesh, float x, float y)
      -> float {
    const auto read = [](const Tessellator::Vertex& vertex) {
      return std::tuple{vertex.x, vertex.y,
                        std::array<float, 4>{vertex.u, vertex.v, 0.0F, 1.0F}};
    };
    std::optional<std::array<float, 4>> best;
    for (std::size_t index = 0; index + 2 < mesh.indices.size(); index += 3) {
      const std::array<std::uint32_t, 3> triangle{mesh.indices[index],
                                                  mesh.indices[index + 1],
                                                  mesh.indices[index + 2]};
      const auto hit =
          probe(std::span<const Tessellator::Vertex>{mesh.vertices},
                std::span<const std::uint32_t>{triangle}, x, y, read);
      if (!hit.has_value()) {
        continue;
      }
      const float u = (*hit)[0];
      const float v = (*hit)[1];
      const float value = std::min(1.0F, (1.0F - std::abs((u * 2.0F) - 1.0F)) *
                                             mesh.multiplier) *
                          std::min(1.0F, v);
      if (!best.has_value() || value > (*best)[0]) {
        best = std::array<float, 4>{value, 0.0F, 0.0F, 0.0F};
      }
    }
    return best.has_value() ? (*best)[0] : 0.0F;
  }

  static auto area(const Tessellator::Mesh& mesh) -> double {
    double total = 0.0;
    for (std::size_t index = 0; index + 2 < mesh.indices.size(); index += 3) {
      const auto& a = mesh.vertices[mesh.indices[index]];
      const auto& b = mesh.vertices[mesh.indices[index + 1]];
      const auto& c = mesh.vertices[mesh.indices[index + 2]];
      if (a.u != 0.5F || b.u != 0.5F || c.u != 0.5F) {
        continue;
      }
      total += std::abs(cross({a.x, a.y}, {b.x, b.y}, {c.x, c.y})) * 0.5;
    }
    return total;
  }

  static auto star() -> graphics::Path {
    graphics::Path path;
    for (int index = 0; index < 5; ++index) {
      const double angle =
          (-std::numbers::pi / 2.0) + (index * 4.0 * std::numbers::pi / 5.0);
      const composition::Point at{
          .x = 50.0F + (40.0F * static_cast<float>(std::cos(angle))),
          .y = 50.0F + (40.0F * static_cast<float>(std::sin(angle)))};
      if (index == 0) {
        path.move(at);
      } else {
        path.add_line(at);
      }
    }
    path.close_subpath();
    return path;
  }

  static auto reference() -> std::map<std::string, std::string> {
    std::map<std::string, std::string> found;
    std::ifstream stream{ARISA_SWIFTUI_PATHS};
    std::string line;
    while (std::getline(stream, line)) {
      std::istringstream fields{line};
      std::string key;
      std::string light;
      if (std::getline(fields, key, '\t') &&
          std::getline(fields, light, '\t')) {
        found.emplace(key, light);
      }
    }
    return found;
  }

  static auto number(const std::map<std::string, std::string>& table,
                     const std::string& key) -> float {
    const auto found = table.find(key);
    EXPECT_NE(found, table.end()) << key;
    return found == table.end() ? 0.0F : std::stof(found->second);
  }

  static auto hex(const std::string& text) -> std::array<float, 3> {
    const auto channel = [&text](std::size_t at) {
      return static_cast<float>(std::stoi(text.substr(at, 2), nullptr, 16)) /
             255.0F;
    };
    return {channel(1), channel(3), channel(5)};
  }

  static auto unpack(std::uint32_t colour) -> std::array<float, 4> {
    return {static_cast<float>(colour & 0xffU) / 255.0F,
            static_cast<float>((colour >> 8U) & 0xffU) / 255.0F,
            static_cast<float>((colour >> 16U) & 0xffU) / 255.0F,
            static_cast<float>(colour >> 24U) / 255.0F};
  }

  static auto mesh_colour(const ShapePainter::Geometry& geometry, float x,
                          float y) -> std::optional<std::array<float, 4>> {
    const auto read = [](const provider::ShapeVertex& vertex) {
      return std::tuple{vertex.position[0], vertex.position[1],
                        unpack(vertex.colour)};
    };
    return probe(std::span<const provider::ShapeVertex>{geometry.vertices},
                 std::span<const std::uint32_t>{geometry.indices}, x, y, read);
  }

  static auto reference_mesh(bool warped) -> ShapePainter::MeshState {
    ShapePainter::MeshState state{
        .width = 3, .height = 3, .points = {}, .colours = {}};
    for (int row = 0; row < 3; ++row) {
      for (int column = 0; column < 3; ++column) {
        state.points.push_back(
            composition::Point{.x = static_cast<float>(column) * 0.5F,
                               .y = static_cast<float>(row) * 0.5F});
      }
    }
    if (warped) {
      state.points[4] = composition::Point{.x = 0.7F, .y = 0.3F};
    }
    state.colours = {theme::Colour{1, 0, 0, 1},   theme::Colour{0, 1, 0, 1},
                     theme::Colour{0, 0, 1, 1},   theme::Colour{1, 1, 0, 1},
                     theme::Colour{1, 1, 1, 1},   theme::Colour{0, 0, 0, 1},
                     theme::Colour{0, 1, 1, 1},   theme::Colour{1, 0, 1, 1},
                     theme::Colour{1, 0.5F, 0, 1}};
    return state;
  }
};

TEST_F(PathTest, AnyShapeTransformsInsetsAndUnevenCorners) {
  const composition::Rect rect{
      .x = 0.0F, .y = 0.0F, .width = 80.0F, .height = 40.0F};
  const graphics::AnyShape plain{theme::Rectangle{}};
  const composition::Rect inset = plain.inset(10.0F).path(rect).bounding_rect();
  EXPECT_FLOAT_EQ(inset.x, 10.0F);
  EXPECT_FLOAT_EQ(inset.width, 60.0F);
  const composition::Rect scaled = plain.scale(0.5F).path(rect).bounding_rect();
  EXPECT_FLOAT_EQ(scaled.x, 20.0F);
  EXPECT_FLOAT_EQ(scaled.height, 20.0F);
  const composition::Rect anchored =
      plain.scale(0.5F, 0.5F, motion::UnitPoint::top_leading)
          .path(rect)
          .bounding_rect();
  EXPECT_FLOAT_EQ(anchored.x, 0.0F);
  EXPECT_FLOAT_EQ(anchored.width, 40.0F);
  const composition::Rect moved =
      graphics::AnyShape{
          graphics::OffsetShape{plain, {.width = 5.0F, .height = 3.0F}}}
          .path(rect)
          .bounding_rect();
  EXPECT_FLOAT_EQ(moved.x, 5.0F);
  EXPECT_FLOAT_EQ(moved.y, 3.0F);
  const composition::Rect turned =
      plain.rotation(motion::Angle::degrees(90.0)).path(rect).bounding_rect();
  EXPECT_NEAR(turned.width, 40.0F, 0.01F);
  EXPECT_NEAR(turned.height, 80.0F, 0.01F);
  const composition::Rect transformed =
      graphics::AnyShape{
          graphics::TransformedShape{
              plain, graphics::AffineTransform::scale(2.0F, 1.0F)}}
          .path(rect)
          .bounding_rect();
  EXPECT_FLOAT_EQ(transformed.width, 160.0F);
  EXPECT_EQ(plain.scale(0.5F), plain.scale(0.5F));
  EXPECT_FALSE(plain.scale(0.5F) == plain.scale(0.6F));
  EXPECT_EQ(graphics::AnyShape{theme::Ellipse{}}.scale(0.5F).leaf(),
            std::optional<theme::Shape>{theme::Ellipse{}});
  const graphics::Path uneven = graphics::Path::shaped(
      theme::UnevenRoundedRectangle{0.0F, 0.0F, 10.0F, 0.0F}, rect,
      theme::RectangleCornerRadii{.bottom_trailing = 10.0F});
  EXPECT_EQ(uneven.bounding_rect(), rect);
  const auto curves = std::ranges::count_if(
      uneven.elements(), [](const graphics::Path::Element& element) {
        return std::holds_alternative<graphics::Path::Curve>(element);
      });
  EXPECT_EQ(curves, 1);
}

TEST_F(PathTest, StyleHierarchyDerivesAndRepeatsLevels) {
  using graphics::HierarchicalShapeStyle;
  const graphics::StyleHierarchy single{theme::Colour::hex(0xFF0000)};
  EXPECT_FLOAT_EQ(
      single.level(HierarchicalShapeStyle::secondary).colour()->opacity(),
      0.5F);
  EXPECT_FLOAT_EQ(
      single.level(HierarchicalShapeStyle::quinary).colour()->opacity(), 0.18F);
  const graphics::StyleHierarchy two{theme::ColourRole::red,
                                     theme::ColourRole::blue};
  EXPECT_EQ(two.level(HierarchicalShapeStyle::quaternary),
            graphics::ShapeStyle{theme::ColourRole::blue});
  const graphics::ShapeStyle shaded =
      graphics::ShapeStyle{theme::ColourRole::red}.shadow(
          graphics::ShadowStyle::inner(theme::ColourRole::shadow, 4.0F));
  EXPECT_EQ(shaded.shadow()->kind(), graphics::ShadowStyle::Kind::inner);
  EXPECT_EQ(graphics::ShapeStyle{theme::ColourRole::red}.opacity(0.5F),
            graphics::ShapeStyle{
                theme::ColourValue{theme::ColourRole::red}.opacity(0.5F)});
}

TEST_F(PathTest, FlatteningKeepsCurvesWithinAQuarterPixel) {
  Tessellator target = tessellator();
  const auto contours = target.flatten(
      graphics::Path::ellipse_in({.x = 0, .y = 0, .width = 200, .height = 200}),
      1.0F, 10.0F, 10.0F);
  ASSERT_EQ(contours.size(), 1U);
  EXPECT_TRUE(contours.front().closed);
  EXPECT_GT(contours.front().points.size(), 24U);
  for (std::size_t index = 0; index < contours.front().points.size(); ++index) {
    const auto& a = contours.front().points[index];
    const auto& b =
        contours.front().points[(index + 1) % contours.front().points.size()];
    const float radius = std::hypot(a[0] - 110.0F, a[1] - 110.0F);
    EXPECT_NEAR(radius, 100.0F, 0.1F);
    const float mid = std::hypot(((a[0] + b[0]) * 0.5F) - 110.0F,
                                 ((a[1] + b[1]) * 0.5F) - 110.0F);
    EXPECT_GT(mid, 100.0F - 0.5F);
  }
}

TEST_F(PathTest, StrokeCoversItsWidthWithAOnePixelFringe) {
  Tessellator target = tessellator();
  graphics::Path line;
  line.move({20.0F, 50.0F});
  line.add_line({120.0F, 50.0F});
  const auto contours = target.flatten(line, 1.0F, 0.0F, 0.0F);
  const auto butt =
      target.stroke(contours, graphics::StrokeStyle{.line_width = 10.0F}, 1.0F);
  EXPECT_FLOAT_EQ(coverage(butt, 70.0F, 50.0F), 1.0F);
  EXPECT_FLOAT_EQ(coverage(butt, 70.0F, 54.0F), 1.0F);
  EXPECT_NEAR(coverage(butt, 70.0F, 55.0F), 0.5F, 0.05F);
  EXPECT_FLOAT_EQ(coverage(butt, 70.0F, 56.0F), 0.0F);
  EXPECT_FLOAT_EQ(coverage(butt, 124.0F, 50.0F), 0.0F);
  const auto round =
      target.stroke(contours,
                    graphics::StrokeStyle{.line_width = 10.0F,
                                          .line_cap = graphics::LineCap::round},
                    1.0F);
  EXPECT_FLOAT_EQ(coverage(round, 123.0F, 50.0F), 1.0F);
  EXPECT_FLOAT_EQ(coverage(round, 124.0F, 54.0F), 0.0F);
  const auto square = target.stroke(
      contours,
      graphics::StrokeStyle{.line_width = 10.0F,
                            .line_cap = graphics::LineCap::square},
      1.0F);
  EXPECT_FLOAT_EQ(coverage(square, 124.0F, 54.0F), 1.0F);
  const auto doubled =
      target.stroke(contours, graphics::StrokeStyle{.line_width = 10.0F}, 2.0F);
  EXPECT_FLOAT_EQ(coverage(doubled, 70.0F, 58.0F), 1.0F);
}

TEST_F(PathTest, FillsFollowTheWindingRule) {
  Tessellator target = tessellator();
  const auto contours = target.flatten(star(), 1.0F, 0.0F, 0.0F);
  const auto odd = target.fill(contours, graphics::FillStyle{.eo_fill = true});
  const auto nonzero = target.fill(contours, graphics::FillStyle{});
  EXPECT_FLOAT_EQ(coverage(nonzero, 50.0F, 50.0F), 1.0F);
  EXPECT_FLOAT_EQ(coverage(odd, 50.0F, 50.0F), 0.0F);
  EXPECT_FLOAT_EQ(coverage(odd, 50.0F, 14.0F), 1.0F);
  EXPECT_LT(area(odd), area(nonzero) * 0.8);
  const auto circle = target.fill(
      target.flatten(graphics::Path::ellipse_in(
                         {.x = 0, .y = 0, .width = 100, .height = 100}),
                     1.0F, 0.0F, 0.0F),
      graphics::FillStyle{});
  EXPECT_FLOAT_EQ(coverage(circle, 50.0F, 50.0F), 1.0F);
  EXPECT_NEAR(coverage(circle, 100.0F, 50.0F), 0.5F, 0.1F);
  EXPECT_FLOAT_EQ(coverage(circle, 101.5F, 50.0F), 0.0F);
}

TEST_F(PathTest, TrimAndDashFollowSwiftUILengths) {
  const auto table = reference();
  Tessellator target = tessellator();
  graphics::Path corner;
  corner.move({10.0F, 10.0F});
  corner.add_line({110.0F, 10.0F});
  corner.add_line({110.0F, 60.0F});
  const auto contours = target.flatten(corner, 1.0F, 0.0F, 0.0F);
  const auto half = Tessellator::trim(contours, 0.0, 0.5);
  ASSERT_EQ(half.size(), 1U);
  EXPECT_NEAR(half.front().points.back()[0],
              number(table, "path.trim.corner_half.max_x"), 0.5F);
  const auto tail = Tessellator::trim(contours, 0.5, 1.0);
  ASSERT_EQ(tail.size(), 1U);
  EXPECT_NEAR(tail.front().points.front()[0],
              number(table, "path.trim.corner_tail.min_x"), 0.5F);
  graphics::Path curve;
  curve.move({10.0F, 80.0F});
  curve.add_curve({120.0F, 80.0F}, {10.0F, 0.0F}, {30.0F, 0.0F});
  const auto bent =
      Tessellator::trim(target.flatten(curve, 2.0F, 0.0F, 0.0F), 0.0, 0.5);
  ASSERT_EQ(bent.size(), 1U);
  EXPECT_NEAR((bent.front().points.back()[0] / 2.0F) + 1.0F,
              number(table, "path.trim.curve_half.max_x"), 1.5F);
  EXPECT_TRUE(Tessellator::trim(contours, 0.6, 0.4).empty());
  graphics::Path line;
  line.move({10.0F, 45.0F});
  line.add_line({110.0F, 45.0F});
  const std::array<float, 2> pattern{10.0F, 5.0F};
  const auto dashes = Tessellator::dash(
      Tessellator::trim(target.flatten(line, 1.0F, 0.0F, 0.0F), 0.0, 0.2),
      pattern, 3.0F);
  ASSERT_EQ(dashes.size(), 2U);
  EXPECT_NEAR(dashes.front().points.front()[0],
              number(table, "path.dash.first.min_x"), 0.5F);
  EXPECT_NEAR(dashes.front().points.back()[0], 17.0F, 0.5F);
  EXPECT_NEAR(dashes.back().points.front()[0], 22.0F, 0.5F);
  EXPECT_NEAR(dashes.back().points.back()[0],
              number(table, "path.dash.first.max_x"), 0.5F);
}

TEST_F(PathTest, ArcsSweepClockwiseOnScreenLikeSwiftUI) {
  const auto table = reference();
  graphics::Path arc;
  arc.add_arc({60.0F, 45.0F}, 40.0F, motion::Angle::degrees(0.0),
              motion::Angle::degrees(90.0), false);
  const composition::Rect bounds = arc.bounding_rect();
  EXPECT_NEAR(bounds.x,
              number(table, "path.arc.counterclockwise_flag.min_x") + 1.0F,
              1.5F);
  EXPECT_NEAR(bounds.y,
              number(table, "path.arc.counterclockwise_flag.min_y") + 1.0F,
              1.5F);
  EXPECT_NEAR(bounds.x + bounds.width,
              number(table, "path.arc.counterclockwise_flag.max_x") - 1.0F,
              1.5F);
}

TEST_F(PathTest, ShapeViewsPaintThroughThePainterWithGradients) {
  provider::headless::NullGpu gpu;
  auto created = Painter::create(gpu);
  ASSERT_TRUE(created.has_value());
  const auto layout = tree(components::VStack(
      composition::Element{components::ShapeView(theme::Circle{})
                               .fill(theme::Colour::hex(0xFF0000))}
          .frame({.width = 80.0F, .height = 80.0F}),
      composition::Element{
          components::ShapeView(theme::Rectangle{})
              .fill(graphics::LinearGradient{
                  .gradient =
                      graphics::Gradient{std::vector<theme::ColourValue>{
                          theme::Colour::hex(0x0AB9E6),
                          theme::Colour::hex(0xFF3C28)}}})}
          .frame({.width = 120.0F, .height = 40.0F})));
  auto context = layout::LayoutContext::create();
  ASSERT_TRUE(context.has_value());
  ASSERT_TRUE(context->update(*layout, handheld, std::nullopt).has_value());
  auto frame = gpu.begin_frame();
  ASSERT_TRUE(frame.has_value());
  ASSERT_TRUE(created
                  ->paint(*layout, *frame,
                          Painter::Target{.pixels = handheld.pixels,
                                          .scale = handheld.scale,
                                          .time = std::chrono::nanoseconds{0}},
                          std::nullopt)
                  .has_value());
  ASSERT_TRUE(frame->present().has_value());
  const auto record = gpu.last_shapes();
  const auto paths = std::ranges::count(
      record.vertices, provider::ShapeKind::path, &provider::ShapeVertex::kind);
  const auto gradients =
      std::ranges::count(record.vertices, provider::ShapeKind::gradient,
                         &provider::ShapeVertex::kind);
  EXPECT_GT(paths, 20);
  EXPECT_GT(gradients, 3);
  EXPECT_EQ(record.vertices.size() % 4, 0U);
}

TEST_F(PathTest, MeshGradientMatchesSwiftUISamples) {
  const auto table = reference();
  provider::headless::NullGpu gpu;
  ShapePainter painter{gpu};
  constexpr float extent = 320.0F;
  constexpr std::array<float, 5> stops{0.1F, 0.25F, 0.5F, 0.75F, 0.9F};
  const auto area = ShapePainter::Area{.left = 0.0F,
                                       .top = 0.0F,
                                       .right = extent,
                                       .bottom = extent,
                                       .scale = 2.0F};
  for (const auto& [name, smooth, warped, tolerance] :
       {std::tuple{"smooth", true, false, 0.03F},
        std::tuple{"linear", false, false, 0.03F},
        std::tuple{"warped", true, true, 0.16F}}) {
    const ShapePainter::Geometry geometry =
        painter.mesh(ShapePainter::MeshRequest{
            .state = reference_mesh(warped),
            .smooth = smooth,
            .space = graphics::Gradient::ColorSpace::device,
            .grain = 0.0F,
            .area = area,
            .opacity = 1.0F});
    float total = 0.0F;
    for (std::size_t row = 0; row < stops.size(); ++row) {
      for (std::size_t column = 0; column < stops.size(); ++column) {
        const float x = (std::floor(extent * stops[column]) + 0.5F);
        const float y = (std::floor(extent * stops[row]) + 0.5F);
        const auto got = mesh_colour(geometry, x, y);
        ASSERT_TRUE(got.has_value());
        const auto want =
            hex(table.at(std::format("path.mesh.{}.{}_{}", name, column, row)));
        float worst = 0.0F;
        for (std::size_t channel = 0; channel < 3; ++channel) {
          worst = std::max(worst, std::abs((*got)[channel] - want[channel]));
        }
        total += worst;
        EXPECT_LE(worst, tolerance) << name << " " << column << "," << row;
      }
    }
    EXPECT_LE(total / 25.0F, warped ? 0.04F : 0.02F) << name;
  }
}

TEST_F(PathTest, MeshAnimationBlendsFromWhatWasShown) {
  provider::headless::NullGpu gpu;
  ShapePainter painter{gpu};
  ShapePainter::MeshState first = reference_mesh(false);
  ShapePainter::MeshState second = reference_mesh(true);
  ShapePainter::Memory& memory =
      painter.remember(composition::NodeId{7}, first);
  EXPECT_EQ(memory.generation, 1U);
  ShapePainter::Memory& changed =
      painter.remember(composition::NodeId{7}, second);
  EXPECT_EQ(changed.generation, 2U);
  ShapePainter::blend(changed, 0.5);
  const ShapePainter::MeshState& halfway = changed.shown;
  EXPECT_NEAR(halfway.points[4].x, 0.6F, 1.0e-5F);
  EXPECT_NEAR(halfway.points[4].y, 0.4F, 1.0e-5F);
}

}
