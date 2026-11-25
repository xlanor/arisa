#include <arisa/components/mesh_gradient.hpp>
#include <arisa/composition/length.hpp>

#include <algorithm>
#include <cstddef>
#include <utility>

namespace arisa::components {

MeshGradient::MeshGradient(int width, int height,
                           std::vector<composition::Point> points,
                           std::vector<theme::ColourValue> colors,
                           theme::ColourValue background, bool smooths_colors,
                           graphics::Gradient::ColorSpace color_space)
    : Element(std::make_unique<Content>(width, height, std::move(points),
                                        std::move(colors), background,
                                        smooths_colors, color_space)) {
  edit_modifiers().frame.max_width = composition::infinity;
  edit_modifiers().frame.max_height = composition::infinity;
}

auto MeshGradient::Colors::values() const -> std::vector<theme::ColourValue> {
  if (const auto* plain =
          std::get_if<std::vector<theme::ColourValue>>(&value_)) {
    return *plain;
  }
  const auto& resolved = std::get<std::vector<theme::Colour>>(value_);
  return {resolved.begin(), resolved.end()};
}

MeshGradient::MeshGradient(int width, int height, Locations locations,
                           Colors colors, theme::ColourValue background,
                           bool smooths_colors,
                           graphics::Gradient::ColorSpace color_space)
    : MeshGradient(width, height, std::vector<composition::Point>{},
                   colors.values(), background, smooths_colors, color_space) {
  if (const auto* points =
          std::get_if<std::vector<composition::Point>>(&locations.value())) {
    edit<Content>() =
        Content{width,      height,         *points,    colors.values(),
                background, smooths_colors, color_space};
  } else {
    const auto& bezier = std::get<std::vector<BezierPoint>>(locations.value());
    std::vector<composition::Point> corners;
    corners.reserve(bezier.size());
    for (const BezierPoint& point : bezier) {
      corners.push_back(point.position);
    }
    edit<Content>() =
        Content{width,      height,         std::move(corners), colors.values(),
                background, smooths_colors, color_space};
    edit<Content>().set_bezier_points(bezier);
  }
}

MeshGradient::MeshGradient(int width, int height,
                           std::vector<BezierPoint> bezier_points,
                           std::vector<theme::ColourValue> colors,
                           theme::ColourValue background, bool smooths_colors,
                           graphics::Gradient::ColorSpace color_space)
    : MeshGradient(width, height,
                   Locations::bezier_points(std::move(bezier_points)),
                   Colors::colors(std::move(colors)), background,
                   smooths_colors, color_space) {}

MeshGradient::MeshGradient(int width, int height,
                           std::vector<BezierPoint> bezier_points,
                           std::vector<theme::Colour> resolved_colors,
                           theme::ColourValue background, bool smooths_colors,
                           graphics::Gradient::ColorSpace color_space)
    : MeshGradient(width, height,
                   Locations::bezier_points(std::move(bezier_points)),
                   Colors::resolved_colors(std::move(resolved_colors)),
                   background, smooths_colors, color_space) {}

auto MeshGradient::grain(float amount) && -> MeshGradient {
  edit<Content>().set_grain(std::clamp(amount, 0.0F, 1.0F));
  return std::move(*this);
}

auto MeshGradient::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto MeshGradient::Content::same_as(
    const composition::ElementContent& other) const -> bool {
  return other.type() == type() && static_cast<const Content&>(other) == *this;
}

auto MeshGradient::Content::paints_like(
    const composition::ElementContent& other) const -> bool {
  return same_as(other);
}

auto MeshGradient::Content::cubic(composition::Point from,
                                  composition::Point first,
                                  composition::Point second,
                                  composition::Point to, float t) noexcept
    -> composition::Point {
  const float u = 1.0F - t;
  const float a = u * u * u;
  const float b = 3.0F * u * u * t;
  const float c = 3.0F * u * t * t;
  const float d = t * t * t;
  return composition::Point{
      .x = (a * from.x) + (b * first.x) + (c * second.x) + (d * to.x),
      .y = (a * from.y) + (b * first.y) + (c * second.y) + (d * to.y)};
}

auto MeshGradient::Content::patch(const MeshGradient::BezierPoint& top_left,
                                  const MeshGradient::BezierPoint& top_right,
                                  const MeshGradient::BezierPoint& bottom_left,
                                  const MeshGradient::BezierPoint& bottom_right,
                                  float u, float v) noexcept
    -> composition::Point {
  const composition::Point top =
      cubic(top_left.position, top_left.trailing_control_point,
            top_right.leading_control_point, top_right.position, u);
  const composition::Point bottom =
      cubic(bottom_left.position, bottom_left.trailing_control_point,
            bottom_right.leading_control_point, bottom_right.position, u);
  const composition::Point left =
      cubic(top_left.position, top_left.bottom_control_point,
            bottom_left.top_control_point, bottom_left.position, v);
  const composition::Point right =
      cubic(top_right.position, top_right.bottom_control_point,
            bottom_right.top_control_point, bottom_right.position, v);
  const auto corner = [&](auto field) {
    return ((1.0F - u) * (1.0F - v) * field(top_left.position)) +
           (u * (1.0F - v) * field(top_right.position)) +
           ((1.0F - u) * v * field(bottom_left.position)) +
           (u * v * field(bottom_right.position));
  };
  const float x = ((1.0F - v) * top.x) + (v * bottom.x) +
                  ((1.0F - u) * left.x) + (u * right.x) -
                  corner([](composition::Point point) { return point.x; });
  const float y = ((1.0F - v) * top.y) + (v * bottom.y) +
                  ((1.0F - u) * left.y) + (u * right.y) -
                  corner([](composition::Point point) { return point.y; });
  return composition::Point{.x = x, .y = y};
}

auto MeshGradient::Content::expanded(
    std::span<const theme::Colour> colours) const -> MeshGradient::Grid {
  if (bezier_.empty()) {
    return MeshGradient::Grid{.width = width_,
                              .height = height_,
                              .points = points_,
                              .colours = {colours.begin(), colours.end()}};
  }
  constexpr int steps = MeshGradient::bezier_steps;
  const int wide = ((width_ - 1) * steps) + 1;
  const int tall = ((height_ - 1) * steps) + 1;
  MeshGradient::Grid grid{
      .width = wide, .height = tall, .points = {}, .colours = {}};
  grid.points.reserve(static_cast<std::size_t>(wide) *
                      static_cast<std::size_t>(tall));
  grid.colours.reserve(grid.points.capacity());
  const auto at = [this](int column, int row) {
    return static_cast<std::size_t>((row * width_) + column);
  };
  for (int row = 0; row < tall; ++row) {
    const int cell_row = std::min(row / steps, height_ - 2);
    const float v = static_cast<float>(row - (cell_row * steps)) /
                    static_cast<float>(steps);
    for (int column = 0; column < wide; ++column) {
      const int cell_column = std::min(column / steps, width_ - 2);
      const float u = static_cast<float>(column - (cell_column * steps)) /
                      static_cast<float>(steps);
      const std::size_t a = at(cell_column, cell_row);
      const std::size_t b = at(cell_column + 1, cell_row);
      const std::size_t c = at(cell_column, cell_row + 1);
      const std::size_t d = at(cell_column + 1, cell_row + 1);
      grid.points.push_back(
          patch(bezier_[a], bezier_[b], bezier_[c], bezier_[d], u, v));
      const theme::Colour upper =
          graphics::Gradient::mix(colours[a], colours[b], u, space_);
      const theme::Colour lower =
          graphics::Gradient::mix(colours[c], colours[d], u, space_);
      grid.colours.push_back(graphics::Gradient::mix(upper, lower, v, space_));
    }
  }
  return grid;
}

auto MeshGradient::Content::well_formed() const noexcept -> bool {
  if (width_ < 2 || height_ < 2) {
    return false;
  }
  const auto count =
      static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_);
  return points_.size() == count && colors_.size() == count &&
         (bezier_.empty() || bezier_.size() == count);
}

}
