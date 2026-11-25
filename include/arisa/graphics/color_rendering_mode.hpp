#pragma once

#include <cstdint>

namespace arisa::graphics {

enum class ColorRenderingMode : std::uint8_t {
  non_linear,
  linear,
  extended_linear,
};

}
