#pragma once

#include <cstdint>

namespace arisa::composition {

enum class DialogSeverity : std::uint8_t {
  automatic,
  standard,
  critical,
};

}
