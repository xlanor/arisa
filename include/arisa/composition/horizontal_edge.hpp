#pragma once

#include <array>
#include <cstdint>

namespace arisa::composition {

enum class HorizontalEdge : std::uint8_t {
  leading,
  trailing,
};

inline constexpr std::array<HorizontalEdge, 2> all_horizontal_edges{
    HorizontalEdge::leading, HorizontalEdge::trailing};

}
