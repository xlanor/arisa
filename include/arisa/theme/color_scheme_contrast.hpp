#pragma once

#include <array>
#include <cstdint>

namespace arisa::theme {

enum class ColorSchemeContrast : std::uint8_t {
  standard,
  increased,
};

inline constexpr std::array<ColorSchemeContrast, 2> all_color_scheme_contrasts{
    ColorSchemeContrast::standard, ColorSchemeContrast::increased};

}
