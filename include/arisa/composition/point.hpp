#pragma once

namespace arisa::composition {

struct Point {
  float x{0.0F};
  float y{0.0F};

  friend constexpr auto operator==(const Point&, const Point&)
      -> bool = default;
};

}
