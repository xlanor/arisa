#pragma once

#include <cstdint>
#include <utility>

namespace arisa::composition {

enum class GestureMask : std::uint8_t {
  none = 0,
  gesture = 1,
  subviews = 2,
  all = 3,
};

constexpr auto operator|(GestureMask left, GestureMask right) noexcept
    -> GestureMask {
  return static_cast<GestureMask>(std::to_underlying(left) |
                                  std::to_underlying(right));
}

constexpr auto operator&(GestureMask left, GestureMask right) noexcept
    -> GestureMask {
  return static_cast<GestureMask>(std::to_underlying(left) &
                                  std::to_underlying(right));
}

}
