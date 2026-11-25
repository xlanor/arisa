#pragma once

#include <arisa/text/font_face.hpp>
#include <arisa/text/point_size.hpp>

#include <cstdint>
#include <memory>

namespace arisa::text {

struct GlyphRun {
  std::shared_ptr<const FontFace> face;
  PointSize size;
  bool right_to_left{false};
  std::uint32_t text_start{0};
  std::uint32_t text_end{0};
  std::uint32_t first_glyph{0};
  std::uint32_t glyph_count{0};
};

}
