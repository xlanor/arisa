#pragma once

#include <cstdint>

namespace arisa::composition {

enum class ScrollIndicatorVisibility : std::uint8_t {
  automatic,
  visible,
  hidden,
  never,
};

}
