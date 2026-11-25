#include <arisa/text/software_renderer.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace arisa::text {

auto SoftwareRenderer::channel(float value) noexcept -> std::uint8_t {
  return static_cast<std::uint8_t>(
      std::lround(std::clamp(value, 0.0F, 1.0F) * 255.0F));
}

auto SoftwareRenderer::blend(std::byte under, float over, float alpha) noexcept
    -> std::byte {
  const float base =
      static_cast<float>(std::to_integer<std::uint8_t>(under)) / 255.0F;
  return std::byte{channel((base * (1.0F - alpha)) + (over * alpha))};
}

auto SoftwareRenderer::render(const TextLayout& layout, GlyphAtlas& atlas,
                              provider::Colour foreground,
                              provider::Colour background, std::uint32_t margin)
    -> std::expected<provider::Image, TextError> {
  const float scale = layout.scale().value();
  const auto width =
      static_cast<std::uint32_t>(std::ceil(layout.size().width * scale)) +
      (2 * margin);
  const auto height =
      static_cast<std::uint32_t>(std::ceil(layout.size().height * scale)) +
      (2 * margin);
  const std::uint32_t columns = std::max<std::uint32_t>(width, 1);
  const std::uint32_t rows = std::max<std::uint32_t>(height, 1);
  std::vector<std::byte> rgba(static_cast<std::size_t>(columns) * rows *
                              provider::Image::channels);
  const std::array fill{std::byte{channel(background.red)},
                        std::byte{channel(background.green)},
                        std::byte{channel(background.blue)},
                        std::byte{channel(background.alpha)}};
  for (std::size_t index = 0; index < rgba.size(); ++index) {
    rgba[index] = fill[index % provider::Image::channels];
  }
  const std::array colour{foreground.red, foreground.green, foreground.blue};
  const auto offset = static_cast<float>(margin);
  for (const GlyphRun& run : layout.runs()) {
    for (const PositionedGlyph& glyph : layout.glyphs(run)) {
      auto placed = atlas.place(*run.face, glyph.glyph, run.size,
                                layout.scale(), glyph.x, glyph.y);
      if (!placed.has_value()) {
        return std::unexpected(placed.error());
      }
      if (!placed->has_value()) {
        continue;
      }
      const GlyphQuad& quad = **placed;
      const AtlasPage& page = atlas.pages()[quad.page];
      const auto extent = static_cast<float>(page.size());
      const auto texel_x =
          static_cast<std::uint32_t>(std::lround(quad.u0 * extent));
      const auto texel_y =
          static_cast<std::uint32_t>(std::lround(quad.v0 * extent));
      const auto glyph_width =
          static_cast<std::uint32_t>(std::lround(quad.right - quad.left));
      const auto glyph_height =
          static_cast<std::uint32_t>(std::lround(quad.bottom - quad.top));
      const long origin_x = std::lround(quad.left + offset);
      const long origin_y = std::lround(quad.top + offset);
      for (std::uint32_t row = 0; row < glyph_height; ++row) {
        const long target_y = origin_y + row;
        if (target_y < 0 || target_y >= rows) {
          continue;
        }
        for (std::uint32_t column = 0; column < glyph_width; ++column) {
          const long target_x = origin_x + column;
          if (target_x < 0 || target_x >= columns) {
            continue;
          }
          const float alpha = static_cast<float>(page.coverage(texel_x + column,
                                                               texel_y + row)) /
                              255.0F;
          if (alpha <= 0.0F) {
            continue;
          }
          const std::size_t base =
              ((static_cast<std::size_t>(target_y) * columns) +
               static_cast<std::size_t>(target_x)) *
              provider::Image::channels;
          for (std::size_t component = 0; component < colour.size();
               ++component) {
            rgba[base + component] =
                blend(rgba[base + component], colour[component], alpha);
          }
        }
      }
    }
  }
  auto image = provider::Image::create(columns, rows, std::move(rgba));
  if (!image.has_value()) {
    return std::unexpected(TextError::from(image.error()));
  }
  return std::move(*image);
}

}
