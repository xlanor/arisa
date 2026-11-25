#pragma once

#include <array>
#include <cstdint>

namespace arisa::theme {

enum class ColorScheme : std::uint8_t {
  light,
  dark,
};

inline constexpr std::array<ColorScheme, 2> all_color_schemes{
    ColorScheme::light, ColorScheme::dark};

}
