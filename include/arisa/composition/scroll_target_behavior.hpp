#pragma once

#include <cstdint>

namespace arisa::composition {

enum class ScrollTargetBehavior : std::uint8_t {
  standard,
  paging,
  view_aligned,
};

}
