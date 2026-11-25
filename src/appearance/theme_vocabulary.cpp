#include <arisa/appearance/theme_vocabulary.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <utility>

namespace arisa::appearance {

auto ThemeVocabulary::colour_roles() noexcept
    -> std::span<const std::string_view> {
  static constexpr std::array<std::string_view, theme::colour_role_count> names{
      "label",
      "secondary_label",
      "tertiary_label",
      "quaternary_label",
      "quinary_label",
      "placeholder_text",
      "link",
      "text",
      "header_text",
      "window_frame_text",
      "control",
      "control_text",
      "disabled_control_text",
      "selected_control",
      "selected_control_text",
      "alternate_selected_control_text",
      "selected_text",
      "selected_menu_item_text",
      "unemphasized_selected_text",
      "dark_text",
      "light_text",
      "on_accent",
      "system_background",
      "secondary_system_background",
      "tertiary_system_background",
      "system_grouped_background",
      "secondary_system_grouped_background",
      "tertiary_system_grouped_background",
      "window_background",
      "under_page_background",
      "control_background",
      "text_background",
      "alternating_content_background",
      "alternating_content_background_alternate",
      "scrubber_textured_background",
      "system_fill",
      "secondary_system_fill",
      "tertiary_system_fill",
      "quaternary_system_fill",
      "quinary_system_fill",
      "separator",
      "opaque_separator",
      "grid",
      "highlight",
      "shadow",
      "scrim",
      "accent",
      "accent_pressed",
      "keyboard_focus_indicator",
      "selected_content_background",
      "unemphasized_selected_content_background",
      "selected_text_background",
      "unemphasized_selected_text_background",
      "find_highlight",
      "red",
      "orange",
      "yellow",
      "green",
      "mint",
      "teal",
      "cyan",
      "blue",
      "indigo",
      "purple",
      "pink",
      "brown",
      "gray",
      "gray2",
      "gray3",
      "gray4",
      "gray5",
      "gray6",
  };
  return names;
}

auto ThemeVocabulary::metrics() noexcept -> std::span<const std::string_view> {
  static constexpr std::array<std::string_view, theme::metric_token_count>
      names{
          "stack_spacing",
          "spacer_minimum",
          "button_horizontal_padding",
          "button_vertical_padding",
          "toolbar_horizontal_padding",
          "toolbar_vertical_padding",
          "menu_item_padding",
          "menu_item_height",
          "menu_vertical_padding",
          "text_field_inset",
          "text_field_vertical_inset",
          "text_field_height",
          "list_plain_row_height",
          "list_plain_row_inset",
          "list_grouped_row_height",
          "list_grouped_margin",
          "list_grouped_row_inset",
          "list_grouped_header_gap",
          "list_grouped_footer_gap",
          "list_grouped_section_gap",
          "sidebar_width",
          "sidebar_rail_width",
          "sidebar_row_height",
          "sidebar_selection_inset",
          "sidebar_row_vertical_inset",
          "navigation_bar_height",
          "alert_width",
          "alert_inset",
          "alert_button_height",
          "alert_button_gap",
          "toolbar_group_height",
          "toolbar_group_padding",
          "toolbar_group_spacing",
          "toolbar_group_inset",
          "toolbar_group_gap",
          "sidebar_panel_inset",
          "sidebar_panel_radius",
  };
  return names;
}

auto ThemeVocabulary::spaces() noexcept -> std::span<const std::string_view> {
  static constexpr std::array<std::string_view, theme::space_token_count> names{
      "none", "xxs", "xs", "sm", "md", "lg", "xl", "xxl",
  };
  return names;
}

auto ThemeVocabulary::text_roles() noexcept
    -> std::span<const std::string_view> {
  static constexpr std::array<std::string_view, theme::text_role_count> names{
      "large_title", "title",       "title2",   "title3",
      "headline",    "subheadline", "body",     "callout",
      "footnote",    "caption",     "caption2",
  };
  return names;
}

template <class T>
auto ThemeVocabulary::lookup(std::span<const std::string_view> names,
                             std::string_view name) noexcept
    -> std::optional<T> {
  const auto found = std::ranges::find(names, name);
  if (found == names.end()) {
    return std::nullopt;
  }
  return static_cast<T>(found - names.begin());
}

auto ThemeVocabulary::colour_role(std::string_view name) noexcept
    -> std::optional<theme::ColourRole> {
  return lookup<theme::ColourRole>(colour_roles(), name);
}

auto ThemeVocabulary::name(theme::ColourRole role) noexcept
    -> std::string_view {
  return colour_roles()[std::to_underlying(role)];
}

auto ThemeVocabulary::metric(std::string_view name) noexcept
    -> std::optional<theme::MetricToken> {
  return lookup<theme::MetricToken>(metrics(), name);
}

auto ThemeVocabulary::name(theme::MetricToken token) noexcept
    -> std::string_view {
  return metrics()[std::to_underlying(token)];
}

auto ThemeVocabulary::space(std::string_view name) noexcept
    -> std::optional<theme::SpaceToken> {
  return lookup<theme::SpaceToken>(spaces(), name);
}

auto ThemeVocabulary::text_role(std::string_view name) noexcept
    -> std::optional<theme::TextRole> {
  return lookup<theme::TextRole>(text_roles(), name);
}

auto ThemeVocabulary::visibility(std::string_view name) noexcept
    -> std::optional<composition::NavigationSplitViewVisibility> {
  static constexpr std::array<std::string_view, 5> names{
      "automatic", "all", "double_column", "detail_only", "icons_only"};
  return lookup<composition::NavigationSplitViewVisibility>(names, name);
}

auto ThemeVocabulary::name(
    composition::NavigationSplitViewVisibility visibility) noexcept
    -> std::string_view {
  static constexpr std::array<std::string_view, 5> names{
      "automatic", "all", "double_column", "detail_only", "icons_only"};
  return names[std::to_underlying(visibility)];
}

auto ThemeVocabulary::digit(char value) noexcept -> std::optional<unsigned> {
  if (value >= '0' && value <= '9') {
    return static_cast<unsigned>(value - '0');
  }
  if (value >= 'a' && value <= 'f') {
    return static_cast<unsigned>(value - 'a' + 10);
  }
  if (value >= 'A' && value <= 'F') {
    return static_cast<unsigned>(value - 'A' + 10);
  }
  return std::nullopt;
}

auto ThemeVocabulary::colour(std::string_view text) noexcept
    -> std::optional<theme::Colour> {
  if (!text.starts_with('#') || (text.size() != 7 && text.size() != 9)) {
    return std::nullopt;
  }
  std::uint32_t value = 0;
  for (const char each : text.substr(1)) {
    const std::optional<unsigned> found = digit(each);
    if (!found.has_value()) {
      return std::nullopt;
    }
    value = (value << 4U) | *found;
  }
  if (text.size() == 7) {
    return theme::Colour::hex(value);
  }
  return theme::Colour::hex(value >> 8U,
                            static_cast<float>(value & 0xffU) / 255.0F);
}

auto ThemeVocabulary::hex(theme::Colour colour) -> std::string {
  const auto channel = [](float value) {
    return static_cast<unsigned>(
        std::lround(std::clamp(value, 0.0F, 1.0F) * 255.0F));
  };
  if (colour.alpha >= 1.0F) {
    return std::format("#{:02X}{:02X}{:02X}", channel(colour.red),
                       channel(colour.green), channel(colour.blue));
  }
  return std::format("#{:02X}{:02X}{:02X}{:02X}", channel(colour.red),
                     channel(colour.green), channel(colour.blue),
                     channel(colour.alpha));
}

}
