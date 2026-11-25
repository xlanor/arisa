#pragma once

#include <arisa/composition/point.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/graphics/any_shape.hpp>
#include <arisa/graphics/stroke_style.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>

namespace arisa::composition {

struct PresentedClip {
  std::array<Point, 4> corners{};
  std::array<float, 4> radii{};
  bool ellipse{false};
  std::optional<graphics::AnyShape> shape{};
  graphics::FillStyle fill{};

  auto contains(Point point) const -> bool {
    const float across_x = corners[1].x - corners[0].x;
    const float across_y = corners[1].y - corners[0].y;
    const float down_x = corners[3].x - corners[0].x;
    const float down_y = corners[3].y - corners[0].y;
    const float width = std::hypot(across_x, across_y);
    const float height = std::hypot(down_x, down_y);
    if (width <= 0.0F || height <= 0.0F) {
      return false;
    }
    const float relative_x = point.x - corners[0].x;
    const float relative_y = point.y - corners[0].y;
    const float u = ((relative_x * across_x) + (relative_y * across_y)) / width;
    const float v = ((relative_x * down_x) + (relative_y * down_y)) / height;
    if (u < 0.0F || v < 0.0F || u > width || v > height) {
      return false;
    }
    if (shape.has_value()) {
      return shape
          ->path(Rect{.x = 0.0F, .y = 0.0F, .width = width, .height = height})
          .contains(Point{.x = u, .y = v}, fill.eo_fill);
    }
    if (ellipse) {
      const float half_width = width * 0.5F;
      const float half_height = height * 0.5F;
      const float dx = (u - half_width) / half_width;
      const float dy = (v - half_height) / half_height;
      return (dx * dx) + (dy * dy) <= 1.0F;
    }
    const bool right = u > width * 0.5F;
    const bool bottom = v > height * 0.5F;
    const std::size_t corner = bottom
                                   ? (right ? std::size_t{2} : std::size_t{3})
                                   : (right ? std::size_t{1} : std::size_t{0});
    const float radius = std::min({radii[corner], width * 0.5F, height * 0.5F});
    if (radius <= 0.0F) {
      return true;
    }
    const float centre_x = right ? width - radius : radius;
    const float centre_y = bottom ? height - radius : radius;
    const bool beyond_x = right ? u > centre_x : u < centre_x;
    const bool beyond_y = bottom ? v > centre_y : v < centre_y;
    if (!beyond_x || !beyond_y) {
      return true;
    }
    const float dx = u - centre_x;
    const float dy = v - centre_y;
    return (dx * dx) + (dy * dy) <= radius * radius;
  }

  friend auto operator==(const PresentedClip&, const PresentedClip&)
      -> bool = default;
};

}
