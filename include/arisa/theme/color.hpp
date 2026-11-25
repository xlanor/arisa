#pragma once

#include <arisa/theme/colour_value.hpp>

namespace arisa::theme {

class Color {
 public:
  static constexpr ColourValue clear{Colour::clear};

  static constexpr ColourValue black{Colour::hex(0x000000)};

  static constexpr ColourValue white{Colour::hex(0xFFFFFF)};

  static constexpr ColourValue label{ColourRole::label};

  static constexpr ColourValue secondary_label{ColourRole::secondary_label};

  static constexpr ColourValue tertiary_label{ColourRole::tertiary_label};

  static constexpr ColourValue quaternary_label{ColourRole::quaternary_label};

  static constexpr ColourValue quinary_label{ColourRole::quinary_label};

  static constexpr ColourValue placeholder_text{ColourRole::placeholder_text};

  static constexpr ColourValue link{ColourRole::link};

  static constexpr ColourValue text{ColourRole::text};

  static constexpr ColourValue header_text{ColourRole::header_text};

  static constexpr ColourValue window_frame_text{ColourRole::window_frame_text};

  static constexpr ColourValue control{ColourRole::control};

  static constexpr ColourValue control_text{ColourRole::control_text};

  static constexpr ColourValue disabled_control_text{
      ColourRole::disabled_control_text};

  static constexpr ColourValue selected_control{ColourRole::selected_control};

  static constexpr ColourValue selected_control_text{
      ColourRole::selected_control_text};

  static constexpr ColourValue alternate_selected_control_text{
      ColourRole::alternate_selected_control_text};

  static constexpr ColourValue selected_text{ColourRole::selected_text};

  static constexpr ColourValue selected_menu_item_text{
      ColourRole::selected_menu_item_text};

  static constexpr ColourValue unemphasized_selected_text{
      ColourRole::unemphasized_selected_text};

  static constexpr ColourValue dark_text{ColourRole::dark_text};

  static constexpr ColourValue light_text{ColourRole::light_text};

  static constexpr ColourValue system_background{ColourRole::system_background};

  static constexpr ColourValue secondary_system_background{
      ColourRole::secondary_system_background};

  static constexpr ColourValue tertiary_system_background{
      ColourRole::tertiary_system_background};

  static constexpr ColourValue system_grouped_background{
      ColourRole::system_grouped_background};

  static constexpr ColourValue secondary_system_grouped_background{
      ColourRole::secondary_system_grouped_background};

  static constexpr ColourValue tertiary_system_grouped_background{
      ColourRole::tertiary_system_grouped_background};

  static constexpr ColourValue window_background{ColourRole::window_background};

  static constexpr ColourValue under_page_background{
      ColourRole::under_page_background};

  static constexpr ColourValue control_background{
      ColourRole::control_background};

  static constexpr ColourValue text_background{ColourRole::text_background};

  static constexpr ColourValue alternating_content_background{
      ColourRole::alternating_content_background};

  static constexpr ColourValue alternating_content_background_alternate{
      ColourRole::alternating_content_background_alternate};

  static constexpr ColourValue scrubber_textured_background{
      ColourRole::scrubber_textured_background};

  static constexpr ColourValue system_fill{ColourRole::system_fill};

  static constexpr ColourValue secondary_system_fill{
      ColourRole::secondary_system_fill};

  static constexpr ColourValue tertiary_system_fill{
      ColourRole::tertiary_system_fill};

  static constexpr ColourValue quaternary_system_fill{
      ColourRole::quaternary_system_fill};

  static constexpr ColourValue quinary_system_fill{
      ColourRole::quinary_system_fill};

  static constexpr ColourValue separator{ColourRole::separator};

  static constexpr ColourValue opaque_separator{ColourRole::opaque_separator};

  static constexpr ColourValue grid{ColourRole::grid};

  static constexpr ColourValue highlight{ColourRole::highlight};

  static constexpr ColourValue shadow{ColourRole::shadow};

  static constexpr ColourValue accent{ColourRole::accent};

  static constexpr ColourValue keyboard_focus_indicator{
      ColourRole::keyboard_focus_indicator};

  static constexpr ColourValue selected_content_background{
      ColourRole::selected_content_background};

  static constexpr ColourValue unemphasized_selected_content_background{
      ColourRole::unemphasized_selected_content_background};

  static constexpr ColourValue selected_text_background{
      ColourRole::selected_text_background};

  static constexpr ColourValue unemphasized_selected_text_background{
      ColourRole::unemphasized_selected_text_background};

  static constexpr ColourValue find_highlight{ColourRole::find_highlight};

  static constexpr ColourValue red{ColourRole::red};

  static constexpr ColourValue orange{ColourRole::orange};

  static constexpr ColourValue yellow{ColourRole::yellow};

  static constexpr ColourValue green{ColourRole::green};

  static constexpr ColourValue mint{ColourRole::mint};

  static constexpr ColourValue teal{ColourRole::teal};

  static constexpr ColourValue cyan{ColourRole::cyan};

  static constexpr ColourValue blue{ColourRole::blue};

  static constexpr ColourValue indigo{ColourRole::indigo};

  static constexpr ColourValue purple{ColourRole::purple};

  static constexpr ColourValue pink{ColourRole::pink};

  static constexpr ColourValue brown{ColourRole::brown};

  static constexpr ColourValue gray{ColourRole::gray};

  static constexpr ColourValue gray2{ColourRole::gray2};

  static constexpr ColourValue gray3{ColourRole::gray3};

  static constexpr ColourValue gray4{ColourRole::gray4};

  static constexpr ColourValue gray5{ColourRole::gray5};

  static constexpr ColourValue gray6{ColourRole::gray6};

  static constexpr ColourValue background{ColourRole::window_background};

  static constexpr ColourValue primary{ColourRole::label};

  static constexpr ColourValue tint{ColourRole::accent};

  static constexpr ColourValue accent_color{ColourRole::accent};

  static constexpr ColourValue control_accent{ColourRole::accent};

  static constexpr ColourValue selection{
      ColourRole::selected_content_background};

  static constexpr ColourValue placeholder{ColourRole::placeholder_text};

  static constexpr ColourValue foreground{ColourRole::label};

  static constexpr ColourValue secondary{ColourRole::secondary_label};

  static constexpr ColourValue tertiary{ColourRole::tertiary_label};

  static constexpr ColourValue quaternary{ColourRole::quaternary_label};

  static constexpr ColourValue quinary{ColourRole::quinary_label};
};

}
