#pragma once

#include <cstdint>

namespace arisa::composition {

enum class MenuOrder : std::uint8_t {
  automatic,
  fixed,
  priority,
};

}
