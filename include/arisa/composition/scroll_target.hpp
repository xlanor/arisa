#pragma once

#include <arisa/composition/axes.hpp>
#include <arisa/composition/point.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/composition/size.hpp>
#include <arisa/motion/unit_point.hpp>

#include <optional>
#include <span>

namespace arisa::composition {

struct ScrollTarget {
  Rect rect{};
  std::optional<motion::UnitPoint> anchor;

  friend constexpr auto operator==(const ScrollTarget&, const ScrollTarget&)
      -> bool = default;
};

struct ScrollTargetContext {
  ScrollTarget original_target;
  Size velocity{};
  Size content_size{};
  Size container_size{};
  Axes axes{Axes::vertical};
  Point starting_offset{};
  Point current_offset{};
  std::span<const Rect> aligned_views{};
  float aligned_margin{0.0F};
  bool horizontally_compact{true};
};

}
