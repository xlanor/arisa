#pragma once

#include <array>
#include <cstdint>

namespace arisa::components {

enum class PanelRadius : std::uint8_t {
  small,
  medium,
  large,
  modal,
};

inline constexpr std::array<PanelRadius, 4> all_panel_radii{
    PanelRadius::small, PanelRadius::medium, PanelRadius::large,
    PanelRadius::modal};

}
