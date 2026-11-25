#pragma once

#include <cstdint>

namespace arisa::composition {

enum class NavigationSplitViewVisibility : std::uint8_t {
  automatic,
  all,
  double_column,
  detail_only,
  icons_only,
};

}
