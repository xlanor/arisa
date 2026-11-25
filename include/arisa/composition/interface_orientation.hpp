#pragma once

#include <array>
#include <cstdint>

namespace arisa::composition {

enum class InterfaceOrientation : std::uint8_t {
  portrait,
  portrait_upside_down,
  landscape_left,
  landscape_right,
};

inline constexpr std::array<InterfaceOrientation, 4> all_interface_orientations{
    InterfaceOrientation::portrait, InterfaceOrientation::portrait_upside_down,
    InterfaceOrientation::landscape_left,
    InterfaceOrientation::landscape_right};

}
