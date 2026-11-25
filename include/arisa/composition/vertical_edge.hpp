#pragma once

#include <array>
#include <cstdint>

namespace arisa::composition {

enum class VerticalEdge : std::uint8_t {
  top,
  bottom,
};

inline constexpr std::array<VerticalEdge, 2> all_vertical_edges{
    VerticalEdge::top, VerticalEdge::bottom};

}
