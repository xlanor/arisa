#include <arisa/text/glyph_bitmap.hpp>

#include <utility>

namespace arisa::text {

GlyphBitmap::GlyphBitmap(std::uint32_t width, std::uint32_t height,
                         std::int32_t left, std::int32_t top,
                         std::vector<std::byte> coverage) noexcept
    : width_(width),
      height_(height),
      left_(left),
      top_(top),
      coverage_(std::move(coverage)) {}

}
