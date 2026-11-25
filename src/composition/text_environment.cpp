#include <arisa/composition/text_environment.hpp>

namespace arisa::composition {

const EnvironmentKey<TextAlignment> TextEnvironment::multiline_text_alignment{
    "multiline_text_alignment", TextAlignment::leading};
const EnvironmentKey<LineLimit> TextEnvironment::line_limit{"line_limit",
                                                            LineLimit{}};
const EnvironmentKey<TruncationMode> TextEnvironment::truncation_mode{
    "truncation_mode", TruncationMode::tail};
const EnvironmentKey<float> TextEnvironment::minimum_scale_factor{
    "minimum_scale_factor", 1.0F};
const EnvironmentKey<float> TextEnvironment::line_spacing{"line_spacing", 0.0F};
const EnvironmentKey<bool> TextEnvironment::allows_tightening{
    "allows_tightening", false};
const EnvironmentKey<std::optional<TextCase>> TextEnvironment::text_case{
    "text_case", std::nullopt};
const EnvironmentKey<float> TextEnvironment::kerning{"kerning", 0.0F};
const EnvironmentKey<float> TextEnvironment::tracking{"tracking", 0.0F};
const EnvironmentKey<float> TextEnvironment::baseline_offset{"baseline_offset",
                                                             0.0F};
const EnvironmentKey<bool> TextEnvironment::monospaced_digit{"monospaced_digit",
                                                             false};
const EnvironmentKey<bool> TextEnvironment::italic{"italic", false};
const EnvironmentKey<std::optional<theme::Font::Weight>>
    TextEnvironment::font_weight{"font_weight", std::nullopt};
const EnvironmentKey<std::optional<theme::Font::Design>>
    TextEnvironment::font_design{"font_design", std::nullopt};
const EnvironmentKey<std::optional<theme::Font::Width>>
    TextEnvironment::font_width{"font_width", std::nullopt};
const EnvironmentKey<TextScale> TextEnvironment::text_scale{
    "text_scale", TextScale::standard};
const EnvironmentKey<TextDecoration> TextEnvironment::underline{
    "underline", TextDecoration{}};
const EnvironmentKey<TextDecoration> TextEnvironment::strikethrough{
    "strikethrough", TextDecoration{}};
const EnvironmentKey<std::optional<text::StringCatalog>>
    TextEnvironment::string_catalog{"string_catalog", std::nullopt};
const EnvironmentKey<LegibilityWeight> TextEnvironment::legibility_weight{
    "legibility_weight", LegibilityWeight::regular};
const EnvironmentKey<std::optional<text::LineHeight>>
    TextEnvironment::line_height{"line_height", std::nullopt};
const EnvironmentKey<std::optional<text::Locale>>
    TextEnvironment::typesetting_language{"typesetting_language", std::nullopt};
const EnvironmentKey<WritingDirectionStrategy>
    TextEnvironment::writing_direction{"writing_direction",
                                       WritingDirectionStrategy::default_};
const EnvironmentKey<TextAlignmentStrategy> TextEnvironment::alignment_strategy{
    "alignment_strategy", TextAlignmentStrategy::default_};
const EnvironmentKey<TextVariantPreference> TextEnvironment::text_variant{
    "text_variant", TextVariantPreference::fixed};

const EnvironmentKey<bool> TextEnvironment::publishes_layout{"publishes_layout",
                                                             false};
const EnvironmentKey<bool> TextEnvironment::custom_rendered{"custom_rendered",
                                                            false};
const EnvironmentKey<bool> TextEnvironment::selectable{"text_selectable",
                                                       false};
const EnvironmentKey<TextSelectionAffinity> TextEnvironment::selection_affinity{
    "text_selection_affinity", TextSelectionAffinity::automatic};

}
