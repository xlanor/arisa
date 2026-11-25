#pragma once

#include <cstdint>

namespace arisa::composition {

enum class MenuActionDismissBehavior : std::uint8_t {
  automatic,
  enabled,
  disabled,
};

}
