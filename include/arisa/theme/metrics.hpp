#pragma once

namespace arisa::theme {

struct StackMetrics {
  float spacing{8.0F};
  float text_spacing{0.0F};
  float spacer_minimum{8.0F};
  float padding{16.0F};
  float text_line{16.0F};
};

struct ButtonMetrics {
  float height{24.0F};
  float horizontal_padding{12.0F};
  float vertical_padding{4.0F};
  float corner_radius{6.0F};
  float toolbar_horizontal_padding{8.0F};
  float toolbar_vertical_padding{4.0F};
};

struct SwitchMetrics {
  float width{54.0F};
  float height{24.0F};
  float knob_width{32.0F};
  float knob_height{20.0F};
  float knob_inset{2.0F};
  float label_gap{8.0F};
};

struct CheckboxMetrics {
  float extent{16.0F};
  float corner_radius{5.0F};
  float label_gap{5.0F};
};

struct SliderMetrics {
  float height{16.0F};
  float track_thickness{6.0F};
  float knob_width{20.0F};
  float knob_height{16.0F};
};

struct StepperMetrics {
  float height{24.0F};
  float segment_width{24.0F};
};

struct SegmentedMetrics {
  float height{24.0F};
  float corner_radius{6.0F};
};

struct MenuMetrics {
  float item_height{24.0F};
  float item_padding{12.0F};
  float vertical_padding{5.0F};
};

struct ProgressMetrics {
  float bar_thickness{8.0F};
  float bar_height{20.0F};
  float spinner_extent{32.0F};
};

struct TextFieldMetrics {
  float height{24.0F};
  float corner_radius{5.5F};
  float text_inset{7.0F};
};

struct ListMetrics {
  float plain_row_height{24.0F};
  float plain_row_inset{8.0F};
  float grouped_row_height{37.0F};
  float grouped_margin{20.0F};
  float grouped_row_inset{10.0F};
  float grouped_corner_radius{6.0F};
  float grouped_header_gap{13.0F};
  float grouped_footer_gap{14.0F};
  float grouped_section_gap{33.0F};
  float sidebar_width{144.0F};
  float sidebar_rail_width{64.0F};
  float sidebar_rail_label_linger{2.0F};
  float sidebar_row_height{32.0F};
  float sidebar_selection_inset{10.0F};
  float sidebar_selection_radius{8.0F};
  float sidebar_panel_inset{8.0F};
  float sidebar_panel_radius{18.0F};
};

struct NavigationMetrics {
  float bar_height{52.0F};
  float group_inset{8.0F};
  float group_padding{0.0F};
  float group_spacing{0.0F};
  float group_gap{8.0F};
  float group_height{36.0F};
  float scroll_edge_opacity{0.7F};
};

struct AlertMetrics {
  float width{260.0F};
  float inset{16.0F};
  float button_height{28.0F};
  float button_gap{8.0F};
};

struct FocusRingMetrics {
  float width{3.0F};
  float outset{3.0F};
  float opacity{0.5F};
};

namespace metrics {

inline constexpr StackMetrics stack{};
inline constexpr ButtonMetrics button{};
inline constexpr SwitchMetrics toggle_switch{};
inline constexpr CheckboxMetrics checkbox{};
inline constexpr SliderMetrics slider{};
inline constexpr StepperMetrics stepper{};
inline constexpr SegmentedMetrics segmented{};
inline constexpr MenuMetrics menu{};
inline constexpr ProgressMetrics progress{};
inline constexpr TextFieldMetrics text_field{};
inline constexpr ListMetrics list{};
inline constexpr NavigationMetrics navigation{};
inline constexpr AlertMetrics alert{};
inline constexpr FocusRingMetrics focus_ring{};

}

}
