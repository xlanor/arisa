#include <arisa/text/text_layout.hpp>
#include <text/shaped_text.hpp>
#include <text/shaper.hpp>

#include <utility>

namespace arisa::text {

TextLayout::TextLayout(TextStyle style, LayoutWidth width,
                       provider::SurfaceScale scale, TextSize size,
                       std::vector<TextLine> lines, std::vector<GlyphRun> runs,
                       std::vector<PositionedGlyph> glyphs) noexcept
    : style_(std::move(style)),
      width_(width),
      scale_(scale),
      size_(size),
      lines_(std::move(lines)),
      runs_(std::move(runs)),
      glyphs_(std::move(glyphs)) {}

auto TextLayout::create(const FontCollection& fonts, std::string_view text,
                        const TextStyle& style, LayoutWidth width,
                        provider::SurfaceScale scale)
    -> std::expected<TextLayout, TextError> {
  auto shaper = Shaper::create();
  if (!shaper.has_value()) {
    return std::unexpected(shaper.error());
  }
  auto shaped = ShapedText::create(fonts, text, style, *shaper);
  if (!shaped.has_value()) {
    return std::unexpected(shaped.error());
  }
  return shaped->layout(width, scale, *shaper);
}

auto TextLayout::glyphs(const GlyphRun& run) const noexcept
    -> std::span<const PositionedGlyph> {
  return std::span{glyphs_}.subspan(run.first_glyph, run.glyph_count);
}

auto TextLayout::first_baseline() const noexcept -> float {
  return lines_.empty() ? 0.0F : lines_.front().baseline;
}

auto TextLayout::last_baseline() const noexcept -> float {
  return lines_.empty() ? 0.0F : lines_.back().baseline;
}

}
