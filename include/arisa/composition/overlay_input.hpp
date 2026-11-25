#pragma once

#include <cstdint>

namespace arisa::composition {

enum class OverlayInput : std::uint8_t {
  passthrough,
  modal,
  cooperative,
};

}
