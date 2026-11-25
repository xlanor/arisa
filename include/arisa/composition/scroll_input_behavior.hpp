#pragma once

#include <cstdint>

namespace arisa::composition {

enum class ScrollInputBehavior : std::uint8_t {
  automatic,
  enabled,
  disabled,
};

}
