#pragma once

namespace arisa::composition {

struct Rect {
  float x{0.0F};
  float y{0.0F};
  float width{0.0F};
  float height{0.0F};

  constexpr auto contains(float point_x, float point_y) const noexcept -> bool {
    return point_x >= x && point_y >= y && point_x < x + width &&
           point_y < y + height;
  }

  friend constexpr auto operator==(const Rect&, const Rect&) -> bool = default;
};

}
