#pragma once

#include <cstdint>

namespace arisa::composition {

enum class DragDropPreviewsFormation : std::uint8_t {
  automatic,
  none,
  pile,
  list,
  stack,
};

}
