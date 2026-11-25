#pragma once

#include <cstdint>

namespace arisa::theme {

enum class RGBColorSpace : std::uint8_t {
  srgb,
  srgb_linear,
  display_p3,
};

}
