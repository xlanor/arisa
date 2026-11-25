#pragma once

#include <arisa/theme/typography.hpp>

#include <cstddef>
#include <cstdint>
#include <variant>

namespace arisa::theme {

enum class MetricToken : std::uint8_t {
  stack_spacing,
  spacer_minimum,
  button_horizontal_padding,
  button_vertical_padding,
  toolbar_horizontal_padding,
  toolbar_vertical_padding,
  menu_item_padding,
  menu_item_height,
  menu_vertical_padding,
  text_field_inset,
  text_field_vertical_inset,
  text_field_height,
  list_plain_row_height,
  list_plain_row_inset,
  list_grouped_row_height,
  list_grouped_margin,
  list_grouped_row_inset,
  list_grouped_header_gap,
  list_grouped_footer_gap,
  list_grouped_section_gap,
  sidebar_width,
  sidebar_rail_width,
  sidebar_row_height,
  sidebar_selection_inset,
  sidebar_row_vertical_inset,
  navigation_bar_height,
  alert_width,
  alert_inset,
  alert_button_height,
  alert_button_gap,
  toolbar_group_height,
  toolbar_group_padding,
  toolbar_group_spacing,
  toolbar_group_inset,
  toolbar_group_gap,
  sidebar_panel_inset,
  sidebar_panel_radius,
};

inline constexpr std::size_t metric_token_count = 37;

class ScaledMetric {
 public:
  constexpr ScaledMetric(float value,
                         TextRole relative_to = TextRole::body) noexcept
      : base_(value), relative_to_(relative_to) {}

  constexpr ScaledMetric(MetricToken token,
                         TextRole relative_to = TextRole::body) noexcept
      : base_(token), relative_to_(relative_to) {}

  constexpr auto token() const noexcept -> const MetricToken* {
    return std::get_if<MetricToken>(&base_);
  }

  constexpr auto value() const noexcept -> const float* {
    return std::get_if<float>(&base_);
  }

  constexpr auto relative_to() const noexcept -> TextRole {
    return relative_to_;
  }

  friend constexpr auto operator==(const ScaledMetric&, const ScaledMetric&)
      -> bool = default;

 private:
  std::variant<float, MetricToken> base_;
  TextRole relative_to_;
};

namespace metric {

inline constexpr ScaledMetric stack_spacing{MetricToken::stack_spacing};
inline constexpr ScaledMetric spacer_minimum{MetricToken::spacer_minimum};
inline constexpr ScaledMetric button_horizontal_padding{
    MetricToken::button_horizontal_padding};
inline constexpr ScaledMetric button_vertical_padding{
    MetricToken::button_vertical_padding};
inline constexpr ScaledMetric toolbar_horizontal_padding{
    MetricToken::toolbar_horizontal_padding};
inline constexpr ScaledMetric toolbar_vertical_padding{
    MetricToken::toolbar_vertical_padding};
inline constexpr ScaledMetric menu_item_padding{MetricToken::menu_item_padding};
inline constexpr ScaledMetric menu_item_height{MetricToken::menu_item_height};
inline constexpr ScaledMetric menu_vertical_padding{
    MetricToken::menu_vertical_padding};
inline constexpr ScaledMetric text_field_inset{MetricToken::text_field_inset};
inline constexpr ScaledMetric text_field_vertical_inset{
    MetricToken::text_field_vertical_inset};
inline constexpr ScaledMetric text_field_height{MetricToken::text_field_height};
inline constexpr ScaledMetric list_plain_row_height{
    MetricToken::list_plain_row_height};
inline constexpr ScaledMetric list_plain_row_inset{
    MetricToken::list_plain_row_inset};
inline constexpr ScaledMetric list_grouped_row_height{
    MetricToken::list_grouped_row_height};
inline constexpr ScaledMetric list_grouped_margin{
    MetricToken::list_grouped_margin};
inline constexpr ScaledMetric list_grouped_row_inset{
    MetricToken::list_grouped_row_inset};
inline constexpr ScaledMetric list_grouped_header_gap{
    MetricToken::list_grouped_header_gap};
inline constexpr ScaledMetric list_grouped_footer_gap{
    MetricToken::list_grouped_footer_gap};
inline constexpr ScaledMetric list_grouped_section_gap{
    MetricToken::list_grouped_section_gap};
inline constexpr ScaledMetric sidebar_width{MetricToken::sidebar_width};
inline constexpr ScaledMetric sidebar_rail_width{
    MetricToken::sidebar_rail_width};
inline constexpr ScaledMetric sidebar_row_height{
    MetricToken::sidebar_row_height};
inline constexpr ScaledMetric sidebar_selection_inset{
    MetricToken::sidebar_selection_inset};
inline constexpr ScaledMetric sidebar_row_vertical_inset{
    MetricToken::sidebar_row_vertical_inset};
inline constexpr ScaledMetric navigation_bar_height{
    MetricToken::navigation_bar_height};
inline constexpr ScaledMetric alert_width{MetricToken::alert_width};
inline constexpr ScaledMetric alert_inset{MetricToken::alert_inset};
inline constexpr ScaledMetric alert_button_height{
    MetricToken::alert_button_height};
inline constexpr ScaledMetric alert_button_gap{MetricToken::alert_button_gap};
inline constexpr ScaledMetric toolbar_group_height{
    MetricToken::toolbar_group_height};
inline constexpr ScaledMetric toolbar_group_padding{
    MetricToken::toolbar_group_padding};
inline constexpr ScaledMetric toolbar_group_spacing{
    MetricToken::toolbar_group_spacing};
inline constexpr ScaledMetric toolbar_group_inset{
    MetricToken::toolbar_group_inset};
inline constexpr ScaledMetric toolbar_group_gap{MetricToken::toolbar_group_gap};
inline constexpr ScaledMetric sidebar_panel_inset{
    MetricToken::sidebar_panel_inset};
inline constexpr ScaledMetric sidebar_panel_radius{
    MetricToken::sidebar_panel_radius};

}

}
