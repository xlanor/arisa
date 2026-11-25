#pragma once

#include <cstdint>

namespace arisa::composition {

enum class TextSelectionAffinity : std::uint8_t {
  automatic,
  upstream,
  downstream,
};

}
