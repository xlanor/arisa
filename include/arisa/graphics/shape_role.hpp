#pragma once

#include <cstdint>

namespace arisa::graphics {

enum class ShapeRole : std::uint8_t {
  fill,
  stroke,
  separator,
};

}
