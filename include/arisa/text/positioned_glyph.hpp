#pragma once

#include <arisa/text/glyph_id.hpp>

#include <cstdint>

namespace arisa::text {

struct PositionedGlyph {
  GlyphId glyph{0};
  float x{0.0F};
  float y{0.0F};
  float advance{0.0F};
  std::uint32_t cluster{0};
};

}
