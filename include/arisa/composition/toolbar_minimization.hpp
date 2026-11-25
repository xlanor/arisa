#pragma once

#include <cstdint>

namespace arisa::composition {

enum class ToolbarMinimizationBehavior : std::uint8_t {
  automatic,
  never,
  on_scroll_down,
  on_scroll_up,
};

enum class ToolbarMinimizationRestoration : std::uint8_t {
  automatic,
  at_scroll_edge,
};

enum class ToolbarMinimizationSafeAreaAdjustment : std::uint8_t {
  automatic,
  enabled,
  disabled,
};

}
