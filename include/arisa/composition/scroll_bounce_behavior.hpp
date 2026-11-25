#pragma once

#include <cstdint>

namespace arisa::composition {

enum class ScrollBounceBehavior : std::uint8_t {
  automatic,
  always,
  based_on_size,
};

}
