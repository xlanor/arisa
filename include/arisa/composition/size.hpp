#pragma once

namespace arisa::composition {

struct Size {
  float width{0.0F};
  float height{0.0F};

  friend constexpr auto operator==(const Size&, const Size&) -> bool = default;
};

}
