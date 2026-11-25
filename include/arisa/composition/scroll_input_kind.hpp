#pragma once

#include <cstdint>

namespace arisa::composition {

enum class ScrollInputKind : std::uint8_t {
  touch,
  controller,
  pointer,
};

}
