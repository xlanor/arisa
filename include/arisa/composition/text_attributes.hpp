#pragma once

#include <arisa/composition/legibility_weight.hpp>
#include <arisa/composition/text_selection_affinity.hpp>
#include <arisa/text/drawing_options.hpp>
#include <arisa/text/line_height.hpp>
#include <arisa/text/line_pattern.hpp>
#include <arisa/text/locale.hpp>
#include <arisa/text/string_catalog.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/typography.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace arisa::composition {

enum class TextAlignment : std::uint8_t {
  leading,
  center,
  trailing,
};

inline constexpr std::array<TextAlignment, 3> text_alignment_cases{
    TextAlignment::leading, TextAlignment::center, TextAlignment::trailing};

enum class TextAlignmentStrategy : std::uint8_t {
  default_,
  layout_based,
  writing_direction_based,
};

enum class WritingDirectionStrategy : std::uint8_t {
  default_,
  layout_based,
  content_based,
};

enum class TextVariantPreference : std::uint8_t {
  fixed,
  size_dependent,
};

enum class TextCase : std::uint8_t {
  uppercase,
  lowercase,
};

enum class TruncationMode : std::uint8_t {
  head,
  middle,
  tail,
};

enum class TextScale : std::uint8_t {
  standard,
  secondary,
};

struct TextDecoration {
  using Pattern = text::LinePattern;

  static constexpr std::uint32_t ns_single = 0x01U;
  static constexpr std::uint32_t ns_pattern_mask = 0x0700U;

  bool active{false};
  std::optional<theme::ColourValue> colour{};
  Pattern pattern{Pattern::solid};

  static constexpr auto ns_underline_style(std::uint32_t style) noexcept
      -> TextDecoration {
    TextDecoration decoration{.active = style != 0U};
    switch ((style & ns_pattern_mask) >> 8U) {
      case 1U:
        decoration.pattern = Pattern::dot;
        break;
      case 2U:
        decoration.pattern = Pattern::dash;
        break;
      case 3U:
        decoration.pattern = Pattern::dash_dot;
        break;
      case 4U:
        decoration.pattern = Pattern::dash_dot_dot;
        break;
      default:
        break;
    }
    return decoration;
  }

  friend auto operator==(const TextDecoration&, const TextDecoration&)
      -> bool = default;
};

struct LinesAtLeast {
  std::size_t minimum{1};
};

struct LinesBetween {
  std::size_t minimum{1};
  std::size_t maximum{1};
};

struct LineLimit {
  std::optional<std::size_t> lines;
  bool reserves_space{false};
  std::optional<std::size_t> minimum{};

  friend auto operator==(const LineLimit&, const LineLimit&) -> bool = default;
};

struct TextAttributes {
  TextAlignment multiline_alignment{TextAlignment::leading};
  LineLimit line_limit{};
  TruncationMode truncation{TruncationMode::tail};
  float minimum_scale_factor{1.0F};
  float line_spacing{0.0F};
  bool allows_tightening{false};
  std::optional<TextCase> text_case{};
  float kerning{0.0F};
  float tracking{0.0F};
  float baseline_offset{0.0F};
  bool monospaced_digit{false};
  bool italic{false};
  std::optional<theme::Font::Weight> weight{};
  std::optional<theme::Font::Design> design{};
  std::optional<theme::Font::Width> width{};
  TextScale scale{TextScale::standard};
  TextDecoration underline{};
  TextDecoration strikethrough{};
  text::StringCatalog catalog{};
  std::chrono::seconds zone_offset{0};
  LegibilityWeight legibility_weight{LegibilityWeight::regular};
  std::optional<text::LineHeight> line_height{};
  std::optional<text::Locale> typesetting_language{};
  WritingDirectionStrategy writing_direction{
      WritingDirectionStrategy::default_};
  TextAlignmentStrategy alignment_strategy{TextAlignmentStrategy::default_};
  TextVariantPreference variant{TextVariantPreference::fixed};
  text::DrawingOptions drawing{};
  bool publishes_layout{false};
  bool custom_rendered{false};
  bool selectable{false};
  TextSelectionAffinity selection_affinity{TextSelectionAffinity::automatic};

  friend auto operator==(const TextAttributes&, const TextAttributes&)
      -> bool = default;
};

}
