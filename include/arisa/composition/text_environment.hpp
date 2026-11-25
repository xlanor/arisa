#pragma once

#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/text_attributes.hpp>

#include <optional>

namespace arisa::composition {

class TextEnvironment {
 public:
  static const EnvironmentKey<TextAlignment> multiline_text_alignment;
  static const EnvironmentKey<LineLimit> line_limit;
  static const EnvironmentKey<TruncationMode> truncation_mode;
  static const EnvironmentKey<float> minimum_scale_factor;
  static const EnvironmentKey<float> line_spacing;
  static const EnvironmentKey<bool> allows_tightening;
  static const EnvironmentKey<std::optional<TextCase>> text_case;
  static const EnvironmentKey<float> kerning;
  static const EnvironmentKey<float> tracking;
  static const EnvironmentKey<float> baseline_offset;
  static const EnvironmentKey<bool> monospaced_digit;
  static const EnvironmentKey<bool> italic;
  static const EnvironmentKey<std::optional<theme::Font::Weight>> font_weight;
  static const EnvironmentKey<std::optional<theme::Font::Design>> font_design;
  static const EnvironmentKey<std::optional<theme::Font::Width>> font_width;
  static const EnvironmentKey<TextScale> text_scale;
  static const EnvironmentKey<TextDecoration> underline;
  static const EnvironmentKey<TextDecoration> strikethrough;
  static const EnvironmentKey<std::optional<text::StringCatalog>>
      string_catalog;
  static const EnvironmentKey<LegibilityWeight> legibility_weight;
  static const EnvironmentKey<std::optional<text::LineHeight>> line_height;
  static const EnvironmentKey<std::optional<text::Locale>> typesetting_language;
  static const EnvironmentKey<WritingDirectionStrategy> writing_direction;
  static const EnvironmentKey<TextAlignmentStrategy> alignment_strategy;
  static const EnvironmentKey<TextVariantPreference> text_variant;
  static const EnvironmentKey<bool> publishes_layout;
  static const EnvironmentKey<bool> custom_rendered;
  static const EnvironmentKey<bool> selectable;
  static const EnvironmentKey<TextSelectionAffinity> selection_affinity;
};

}
