#pragma once

#include <arisa/text/font_name.hpp>
#include <arisa/text/font_weight.hpp>
#include <arisa/text/line_height.hpp>
#include <arisa/text/line_pattern.hpp>
#include <arisa/text/point_size.hpp>
#include <arisa/text/small_caps.hpp>
#include <arisa/text/text_alignment.hpp>
#include <arisa/text/text_attachment.hpp>
#include <arisa/text/typeface.hpp>

#include <cstdint>
#include <optional>

namespace arisa::text {

struct TextRun {
  std::uint32_t start{0};
  std::uint32_t end{0};
  std::optional<FontWeight> weight{};
  bool italic{false};
  std::optional<Typeface> typeface{};
  bool underline{false};
  bool strikethrough{false};
  std::uint16_t ink{0};
  std::uint16_t link{0};
  std::optional<PointSize> size{};
  std::optional<float> stretch{};
  std::optional<FontName> name{};
  std::optional<float> kerning{};
  std::optional<float> tracking{};
  std::optional<float> baseline_offset{};
  std::optional<LineHeight> line_height{};
  std::optional<TextAlignment> alignment{};
  std::optional<TextAttachment> attachment{};
  SmallCaps small_caps{SmallCaps::none};
  bool tabular_digits{false};
  LinePattern underline_pattern{LinePattern::solid};
  LinePattern strikethrough_pattern{LinePattern::solid};
  std::uint16_t underline_ink{0};
  std::uint16_t strikethrough_ink{0};
  std::uint16_t background{0};
  std::uint16_t font{0};
  std::uint16_t extra{0};
  std::uint16_t image{0};
  std::uint16_t glyph{0};
  std::uint16_t shade{0};
  std::uint16_t custom{0};

  friend auto operator==(const TextRun&, const TextRun&) -> bool = default;
};

}
