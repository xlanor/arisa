#pragma once

#include <arisa/provider/surface/surface_scale.hpp>
#include <arisa/text/drawing_options.hpp>
#include <arisa/text/font_collection.hpp>
#include <arisa/text/glyph_run.hpp>
#include <arisa/text/layout_width.hpp>
#include <arisa/text/positioned_glyph.hpp>
#include <arisa/text/text_error.hpp>
#include <arisa/text/text_line.hpp>
#include <arisa/text/text_size.hpp>
#include <arisa/text/text_style.hpp>

#include <expected>
#include <span>
#include <string_view>
#include <vector>

namespace arisa::text {

class TextLayout {
 public:
  using DrawingOptions = text::DrawingOptions;

  [[nodiscard]] static auto create(const FontCollection& fonts,
                                   std::string_view text,
                                   const TextStyle& style, LayoutWidth width,
                                   provider::SurfaceScale scale)
      -> std::expected<TextLayout, TextError>;

  auto style() const noexcept -> const TextStyle& { return style_; }

  auto width_constraint() const noexcept -> LayoutWidth { return width_; }

  auto scale() const noexcept -> provider::SurfaceScale { return scale_; }

  auto size() const noexcept -> TextSize { return size_; }

  auto lines() const noexcept -> std::span<const TextLine> { return lines_; }

  auto runs() const noexcept -> std::span<const GlyphRun> { return runs_; }

  auto glyphs() const noexcept -> std::span<const PositionedGlyph> {
    return glyphs_;
  }

  auto glyphs(const GlyphRun& run) const noexcept
      -> std::span<const PositionedGlyph>;

  auto first_baseline() const noexcept -> float;

  auto last_baseline() const noexcept -> float;

 private:
  friend class ShapedText;

  TextLayout(TextStyle style, LayoutWidth width, provider::SurfaceScale scale,
             TextSize size, std::vector<TextLine> lines,
             std::vector<GlyphRun> runs,
             std::vector<PositionedGlyph> glyphs) noexcept;

  TextStyle style_;
  LayoutWidth width_;
  provider::SurfaceScale scale_;
  TextSize size_;
  std::vector<TextLine> lines_;
  std::vector<GlyphRun> runs_;
  std::vector<PositionedGlyph> glyphs_;
};

}
