#pragma once

#include <cstdint>

namespace arisa::composition {

enum class ScrollDismissesKeyboardMode : std::uint8_t {
  automatic,
  immediately,
  interactively,
  never,
};

}
