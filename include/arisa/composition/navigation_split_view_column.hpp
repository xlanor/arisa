#pragma once

#include <cstdint>

namespace arisa::composition {

enum class NavigationSplitViewColumn : std::uint8_t {
  sidebar,
  content,
  detail,
};

}
