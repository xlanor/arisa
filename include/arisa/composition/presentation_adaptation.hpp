#pragma once

#include <cstdint>

namespace arisa::composition {

enum class PresentationAdaptation : std::uint8_t {
  automatic,
  none,
  popover,
  sheet,
  full_screen_cover,
};

}
