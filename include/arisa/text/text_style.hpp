#pragma once

#include <arisa/text/drawing_options.hpp>
#include <arisa/text/font_name.hpp>
#include <arisa/text/font_weight.hpp>
#include <arisa/text/line_height.hpp>
#include <arisa/text/line_pattern.hpp>
#include <arisa/text/locale.hpp>
#include <arisa/text/point_size.hpp>
#include <arisa/text/small_caps.hpp>
#include <arisa/text/text_alignment.hpp>
#include <arisa/text/text_runs.hpp>
#include <arisa/text/typeface.hpp>
#include <arisa/text/writing_direction.hpp>

#include <cstdint>
#include <optional>

namespace arisa::text {

enum class FontDesign : std::uint8_t {
  text,
  display,
};

struct TextStyle {
  PointSize size{PointSize::of<17.0F>()};
  FontWeight weight{font_weight::regular};
  FontDesign design{FontDesign::text};
  float tracking{0.0F};
  float kerning{0.0F};
  float baseline_offset{0.0F};
  float line_spacing{0.0F};
  bool tabular_digits{false};
  std::optional<PointSize> line_height;
  Locale locale{Locale::undetermined()};
  TextAlignment alignment{TextAlignment::natural};
  bool italic{false};
  Typeface typeface{Typeface::standard};
  float stretch{1.0F};
  std::optional<FontName> name{};
  bool underline{false};
  bool strikethrough{false};
  TextRuns runs{};
  SmallCaps small_caps{SmallCaps::none};
  LinePattern underline_pattern{LinePattern::solid};
  LinePattern strikethrough_pattern{LinePattern::solid};
  std::optional<LineHeight> line_rule{};
  std::optional<Locale> language{};
  WritingDirection direction{WritingDirection::natural};
  DrawingOptions drawing{};

  friend auto operator==(const TextStyle&, const TextStyle&) -> bool = default;
};

}
