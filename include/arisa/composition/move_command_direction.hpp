#pragma once

#include <array>
#include <cstdint>

namespace arisa::composition {

enum class MoveCommandDirection : std::uint8_t {
  up,
  down,
  left,
  right,
};

inline constexpr std::array all_move_command_directions{
    MoveCommandDirection::up, MoveCommandDirection::down,
    MoveCommandDirection::left, MoveCommandDirection::right};

}
