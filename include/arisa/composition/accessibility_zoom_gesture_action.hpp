#pragma once

#include <arisa/composition/point.hpp>
#include <arisa/motion/unit_point.hpp>

#include <cstdint>

namespace arisa::composition {

struct AccessibilityZoomGestureAction {
  enum class Direction : std::uint8_t {
    zoom_in,
    zoom_out,
  };

  Direction direction{Direction::zoom_in};
  motion::UnitPoint location{motion::UnitPoint::center};
  Point point{};

  friend constexpr auto operator==(const AccessibilityZoomGestureAction&,
                                   const AccessibilityZoomGestureAction&)
      -> bool = default;
};

}
