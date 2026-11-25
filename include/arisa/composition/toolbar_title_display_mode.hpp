#pragma once

#include <cstdint>

namespace arisa::composition {

enum class ToolbarTitleDisplayMode : std::uint8_t {
  automatic,
  inline_title,
  inline_large,
  large,
};

using NavigationBarTitleDisplayMode = ToolbarTitleDisplayMode;

}
