#pragma once

#include <cstdint>

namespace arisa::composition {

enum class PresentationContentInteraction : std::uint8_t {
  automatic,
  resizes,
  scrolls,
};

}
