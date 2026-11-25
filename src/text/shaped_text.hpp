#pragma once

#include <arisa/provider/surface/surface_scale.hpp>
#include <arisa/text/face_query.hpp>
#include <arisa/text/font_collection.hpp>
#include <arisa/text/font_face.hpp>
#include <arisa/text/layout_width.hpp>
#include <arisa/text/text_attachment.hpp>
#include <arisa/text/text_error.hpp>
#include <arisa/text/text_layout.hpp>
#include <arisa/text/text_size.hpp>
#include <arisa/text/text_style.hpp>
#include <text/bidi_text.hpp>
#include <text/shaped_glyph.hpp>
#include <text/shaper.hpp>
#include <text/text_breaks.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::text {

class ShapedText {
 public:
  [[nodiscard]] static auto create(const FontCollection& fonts,
                                   std::string_view text,
                                   const TextStyle& style, Shaper& shaper)
      -> std::expected<ShapedText, TextError>;

  auto measure(LayoutWidth width, provider::SurfaceScale scale,
               Shaper& shaper) const -> TextSize;

  auto layout(LayoutWidth width, provider::SurfaceScale scale,
              Shaper& shaper) const -> TextLayout;

  auto text() const noexcept -> std::string_view { return *text_; }

  auto style() const noexcept -> const TextStyle& { return style_; }

 private:
  struct Shaping {
    PointSize size{PointSize::of<17.0F>()};
    float spread{0.0F};
    float rise{0.0F};
    bool cased{false};
    bool tabular{false};
    std::optional<TextAttachment> attachment{};

    friend auto operator==(const Shaping&, const Shaping&) -> bool = default;
  };

  struct Run {
    std::uint32_t start{0};
    std::uint32_t end{0};
    std::uint32_t face{0};
    std::uint16_t chain{0};
    std::uint8_t level{0};
    std::uint32_t script{0};
    std::uint32_t first_glyph{0};
    std::uint32_t glyph_count{0};
    Shaping shaping{};
  };

  struct LineSpan {
    std::uint32_t start{0};
    std::uint32_t end{0};
    std::uint32_t visible_end{0};
  };

  struct Piece {
    std::uint32_t run{0};
    std::uint32_t start{0};
    std::uint32_t end{0};
    bool reshaped{false};
    std::vector<ShapedGlyph> glyphs;
    float width{0.0F};
  };

  struct LineBox {
    float width{0.0F};
    float ascent{0.0F};
    float descent{0.0F};
    float height{0.0F};
    float baseline{0.0F};
  };

  static constexpr std::uint8_t cluster_start = 1U;
  static constexpr std::uint8_t cluster_unsafe = 2U;
  static constexpr float fit_tolerance = 0.01F;

  using Chains = std::vector<std::shared_ptr<const FontCollection::Chain>>;

  ShapedText(std::unique_ptr<const std::string> text,
             std::unique_ptr<const std::string> cased, TextStyle style,
             Chains chains, BidiText bidi, TextBreaks breaks,
             std::vector<Run> runs) noexcept;

  static auto shaping_of(const TextStyle& style, std::string_view text,
                         std::size_t start, std::size_t end) -> Shaping;

  static auto cased_text(std::string_view text, const TextStyle& style,
                         const TextBreaks& breaks)
      -> std::unique_ptr<const std::string>;

  static auto small_capital(std::string_view grapheme, SmallCaps caps)
      -> std::optional<bool>;

  auto line_height(const std::vector<Piece>& pieces, float natural) const
      -> std::optional<float>;

  auto alignment_at(std::uint32_t offset) const noexcept -> TextAlignment;

  static auto query(const TextStyle& style) noexcept -> FaceQuery;

  static auto query(const TextStyle& style, const TextRun& run) noexcept
      -> FaceQuery;

  static auto chain_of(const TextStyle& style,
                       std::span<const FaceQuery> queries,
                       std::uint32_t offset) noexcept -> std::uint16_t;

  static auto ignorable(char32_t codepoint) noexcept -> bool;
  static auto control(char32_t codepoint) noexcept -> bool;
  static auto hangs(char32_t codepoint) noexcept -> bool;
  static auto mandatory_break(char32_t codepoint) noexcept -> bool;
  static auto cursive(std::uint32_t script) noexcept -> bool;
  static auto tag(char first, char second, char third, char fourth) noexcept
      -> std::uint32_t;

  static auto private_use_faces(
      const FontCollection& fonts,
      const std::vector<std::shared_ptr<const FontFace>>& chain)
      -> std::vector<std::uint32_t>;

  static auto select_face(
      std::string_view text, std::size_t start, std::size_t end,
      std::optional<std::uint32_t> previous,
      const std::vector<std::shared_ptr<const FontFace>>& chain,
      std::span<const std::uint32_t> private_use) -> std::uint32_t;

  static auto itemise(
      std::string_view text, const TextStyle& style, const BidiText& bidi,
      const TextBreaks& breaks, const std::vector<std::uint32_t>& scripts,
      std::span<const FaceQuery> queries, const Chains& chains,
      const std::vector<std::vector<std::uint32_t>>& private_use)
      -> std::vector<Run>;

  auto face_of(const Run& run) const noexcept -> const FontFace&;

  auto shared_face_of(const Run& run) const noexcept
      -> const std::shared_ptr<const FontFace>&;

  void shape_all(Shaper& shaper);

  auto shape_range(Shaper& shaper, const Run& run, std::uint32_t start,
                   std::uint32_t end, bool right_to_left) const
      -> std::vector<ShapedGlyph>;

  auto finish(std::vector<ShapedGlyph> glyphs, const Run& run) const
      -> std::vector<ShapedGlyph>;

  auto width(std::size_t start, std::size_t end) const noexcept -> float;

  auto trim(std::size_t start, std::size_t end) const noexcept -> std::uint32_t;

  auto wrap(LayoutWidth width) const -> std::vector<LineSpan>;

  auto emergency(std::size_t start, std::size_t limit,
                 float maximum) const noexcept -> std::size_t;

  auto safe_edge(const Run& run, std::size_t offset) const noexcept -> bool;

  auto pieces(const LineSpan& span, Shaper& shaper) const -> std::vector<Piece>;

  auto line_box(const std::vector<Piece>& pieces, float top,
                provider::SurfaceScale scale) const -> LineBox;

  auto run_glyphs(const Run& run) const noexcept
      -> std::span<const ShapedGlyph>;

  std::unique_ptr<const std::string> text_;
  std::unique_ptr<const std::string> cased_;
  TextStyle style_;
  std::string language_;
  Chains chains_;
  BidiText bidi_;
  TextBreaks breaks_;
  std::vector<Run> runs_;
  std::vector<ShapedGlyph> glyphs_;
  std::vector<float> prefix_;
  std::vector<std::uint8_t> clusters_;
};

}
