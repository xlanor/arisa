#pragma once

#include <cstdint>

namespace arisa::composition {

enum class ScrollAnchorRole : std::uint8_t {
  initial_offset,
  size_changes,
  alignment,
};

}
