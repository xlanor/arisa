#pragma once

#include <cstdint>

namespace arisa::theme {

enum class ColourInterpolation : std::uint8_t {
  device,
  perceptual,
  oklch,
  linear,
};

}
