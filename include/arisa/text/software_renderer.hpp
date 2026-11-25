#pragma once

#include <arisa/provider/gpu/colour.hpp>
#include <arisa/provider/gpu/image.hpp>
#include <arisa/text/glyph_atlas.hpp>
#include <arisa/text/text_error.hpp>
#include <arisa/text/text_layout.hpp>

#include <array>
#include <cstdint>
#include <expected>
#include <vector>

namespace arisa::text {

class SoftwareRenderer {
 public:
  [[nodiscard]] static auto render(const TextLayout& layout, GlyphAtlas& atlas,
                                   provider::Colour foreground,
                                   provider::Colour background,
                                   std::uint32_t margin)
      -> std::expected<provider::Image, TextError>;

 private:
  static auto channel(float value) noexcept -> std::uint8_t;

  static auto blend(std::byte under, float over, float alpha) noexcept
      -> std::byte;
};

}
