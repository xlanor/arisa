#pragma once

#include <array>
#include <cstdint>

namespace arisa::composition {

enum class VerticalDirection : std::uint8_t {
  up,
  down,
};

inline constexpr std::array<VerticalDirection, 2> all_vertical_directions{
    VerticalDirection::up, VerticalDirection::down};

}
