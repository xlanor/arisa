#pragma once

#include <array>
#include <cstdint>

namespace arisa::composition {

enum class Axis : std::uint8_t {
  horizontal,
  vertical,
};

inline constexpr std::array<Axis, 2> all_axes{Axis::horizontal, Axis::vertical};

}
