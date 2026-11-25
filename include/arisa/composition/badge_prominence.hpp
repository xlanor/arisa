#pragma once

#include <cstdint>

namespace arisa::composition {

enum class BadgeProminence : std::uint8_t {
  decreased,
  standard,
  increased,
};

}
