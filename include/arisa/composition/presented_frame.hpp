#pragma once

#include <arisa/composition/point.hpp>
#include <arisa/composition/presented_clip.hpp>
#include <arisa/composition/rect.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>

namespace arisa::composition {

struct PresentedFrame {
  std::array<Point, 4> corners{};
  Rect bounds{};
  bool visible{true};
  std::optional<Rect> clip{};
  float hit_radius{0.0F};
  float opacity{1.0F};
  std::optional<PresentedClip> clip_shape{};
  std::optional<PresentedClip> hit_shape{};
  std::optional<std::array<Point, 4>> drawn{};
  bool hollow{false};

  constexpr auto contains(Point point) const noexcept -> bool {
    return encloses(corners, point);
  }

  static constexpr auto encloses(const std::array<Point, 4>& quad,
                                 Point point) noexcept -> bool {
    float sign = 0.0F;
    for (std::size_t index = 0; index < quad.size(); ++index) {
      const Point from = quad[index];
      const Point to = quad[(index + 1) % quad.size()];
      const float cross = ((to.x - from.x) * (point.y - from.y)) -
                          ((to.y - from.y) * (point.x - from.x));
      if (cross == 0.0F) {
        continue;
      }
      if (sign == 0.0F) {
        sign = cross;
      } else if ((sign > 0.0F) != (cross > 0.0F)) {
        return false;
      }
    }
    return sign != 0.0F;
  }

  auto hits(Point point) const noexcept -> bool {
    if (!covers(point)) {
      return false;
    }
    if (hit_shape.has_value() || hit_radius > 0.0F) {
      return true;
    }
    if (hollow) {
      return false;
    }
    return !drawn.has_value() || encloses(*drawn, point);
  }

  auto covers(Point point) const noexcept -> bool {
    if (!visible || (clip.has_value() && !clip->contains(point.x, point.y)) ||
        !contains(point) ||
        (clip_shape.has_value() && !clip_shape->contains(point))) {
      return false;
    }
    if (hit_shape.has_value()) {
      return hit_shape->contains(point);
    }
    if (hit_radius <= 0.0F) {
      return true;
    }
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
    const float radius = std::min({hit_radius, width * 0.5F, height * 0.5F});
    const float dx = std::max({radius - u, 0.0F, u - (width - radius)});
    const float dy = std::max({radius - v, 0.0F, v - (height - radius)});
    return (dx * dx) + (dy * dy) <= radius * radius;
  }

  friend auto operator==(const PresentedFrame&, const PresentedFrame&)
      -> bool = default;
};

}
