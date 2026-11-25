#pragma once

#include <cstdint>

namespace arisa::text {

struct ShapedGlyph {
  std::uint32_t glyph{0};
  std::uint32_t cluster{0};
  float advance{0.0F};
  float x_offset{0.0F};
  float y_offset{0.0F};
  bool unsafe_to_break{false};
};

}
