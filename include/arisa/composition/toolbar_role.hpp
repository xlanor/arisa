#pragma once

#include <cstdint>

namespace arisa::composition {

enum class ToolbarRole : std::uint8_t {
  automatic,
  navigation_stack,
  browser,
  editor,
};

}
