#pragma once

#include <cstdint>

namespace arisa::composition {

enum class ToolbarVerticalBehavior : std::uint8_t {
  automatic,
  disabled,
};

enum class ToolbarVerticalCompressionBehavior : std::uint8_t {
  automatic,
  prefers_tab_bar,
  prefers_toolbar_items,
};

enum class ToolbarItemAxisBehavior : std::uint8_t {
  automatic,
  horizontal_only,
  vertical_preferred,
};

}
