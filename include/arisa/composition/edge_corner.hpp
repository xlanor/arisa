#pragma once

#include <array>
#include <cstdint>

namespace arisa::composition {

enum class EdgeCorner : std::uint8_t {
  top_leading,
  top_trailing,
  bottom_leading,
  bottom_trailing,
};

inline constexpr std::array<EdgeCorner, 4> all_edge_corners{
    EdgeCorner::top_leading, EdgeCorner::top_trailing,
    EdgeCorner::bottom_leading, EdgeCorner::bottom_trailing};

}
