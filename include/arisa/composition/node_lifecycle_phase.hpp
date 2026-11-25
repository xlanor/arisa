#pragma once

#include <cstdint>

namespace arisa::composition {

enum class NodeLifecyclePhase : std::uint8_t {
  mounting,
  live,
  departing,
  retired,
};

}
