#pragma once

#include <cstdint>

namespace arisa::composition {

enum class NavigationSplitViewStyle : std::uint8_t {
  automatic,
  balanced,
  prominent_detail,
};

}
