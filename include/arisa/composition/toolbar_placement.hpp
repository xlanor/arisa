#pragma once

#include <cstdint>

namespace arisa::composition {

enum class ToolbarPlacement : std::uint8_t {
  automatic,
  navigation_bar,
  bottom_bar,
  tab_bar,
  status_bar,
};

}
