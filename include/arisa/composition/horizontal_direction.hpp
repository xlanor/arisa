#pragma once

#include <array>
#include <cstdint>

namespace arisa::composition {

enum class HorizontalDirection : std::uint8_t {
  leading,
  trailing,
};

inline constexpr std::array<HorizontalDirection, 2> all_horizontal_directions{
    HorizontalDirection::leading, HorizontalDirection::trailing};

}
