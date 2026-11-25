#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/theme.hpp>

#include <arisa/theme/appearance_variants.hpp>

#include <array>
#include <span>
#include <string>
#include <string_view>

namespace arisa::examples {

class ColoursPage final : public composition::Component {
 public:
  struct Swatch {
    std::string_view name;
    theme::ColourRole role;
  };

  struct Variant {
    std::string_view name;
    theme::Colour theme::AppearanceVariants::* value;
    theme::Colour theme::AppearanceVariants::* base;
  };

  static constexpr float chip_width = 64.0F;
  static constexpr float chip_height = 28.0F;
  static constexpr float mat_inset = 3.0F;
  static constexpr float mat_split = 0.55F;

  static constexpr std::array variants{
      Variant{"Light", &theme::AppearanceVariants::light, nullptr},
      Variant{"Dark", &theme::AppearanceVariants::dark, nullptr},
      Variant{"Dark elevated", &theme::AppearanceVariants::dark_elevated,
              &theme::AppearanceVariants::dark},
      Variant{"HC light", &theme::AppearanceVariants::high_contrast_light,
              &theme::AppearanceVariants::light},
      Variant{"HC dark", &theme::AppearanceVariants::high_contrast_dark,
              &theme::AppearanceVariants::dark},
      Variant{"HC dark elevated",
              &theme::AppearanceVariants::high_contrast_dark_elevated,
              &theme::AppearanceVariants::high_contrast_dark}};

  static constexpr std::array adaptive{
      Swatch{"label", theme::ColourRole::label},
      Swatch{"secondary_label", theme::ColourRole::secondary_label},
      Swatch{"tertiary_label", theme::ColourRole::tertiary_label},
      Swatch{"quaternary_label", theme::ColourRole::quaternary_label},
      Swatch{"quinary_label", theme::ColourRole::quinary_label},
      Swatch{"placeholder_text", theme::ColourRole::placeholder_text},
      Swatch{"link", theme::ColourRole::link},
      Swatch{"text", theme::ColourRole::text},
      Swatch{"dark_text", theme::ColourRole::dark_text},
      Swatch{"light_text", theme::ColourRole::light_text},
      Swatch{"on_accent", theme::ColourRole::on_accent}};
  static constexpr std::array backgrounds{
      Swatch{"system_background", theme::ColourRole::system_background},
      Swatch{"secondary_system_background",
             theme::ColourRole::secondary_system_background},
      Swatch{"tertiary_system_background",
             theme::ColourRole::tertiary_system_background},
      Swatch{"system_grouped_background",
             theme::ColourRole::system_grouped_background},
      Swatch{"secondary_system_grouped_background",
             theme::ColourRole::secondary_system_grouped_background},
      Swatch{"tertiary_system_grouped_background",
             theme::ColourRole::tertiary_system_grouped_background},
      Swatch{"system_fill", theme::ColourRole::system_fill},
      Swatch{"secondary_system_fill", theme::ColourRole::secondary_system_fill},
      Swatch{"tertiary_system_fill", theme::ColourRole::tertiary_system_fill},
      Swatch{"quaternary_system_fill",
             theme::ColourRole::quaternary_system_fill},
      Swatch{"quinary_system_fill", theme::ColourRole::quinary_system_fill},
      Swatch{"separator", theme::ColourRole::separator},
      Swatch{"opaque_separator", theme::ColourRole::opaque_separator},
      Swatch{"scrim", theme::ColourRole::scrim}};
  static constexpr std::array tints{
      Swatch{"accent", theme::ColourRole::accent},
      Swatch{"accent_pressed", theme::ColourRole::accent_pressed},
      Swatch{"keyboard_focus_indicator",
             theme::ColourRole::keyboard_focus_indicator},
      Swatch{"red", theme::ColourRole::red},
      Swatch{"orange", theme::ColourRole::orange},
      Swatch{"yellow", theme::ColourRole::yellow},
      Swatch{"green", theme::ColourRole::green},
      Swatch{"mint", theme::ColourRole::mint},
      Swatch{"teal", theme::ColourRole::teal},
      Swatch{"cyan", theme::ColourRole::cyan},
      Swatch{"blue", theme::ColourRole::blue},
      Swatch{"indigo", theme::ColourRole::indigo},
      Swatch{"purple", theme::ColourRole::purple},
      Swatch{"pink", theme::ColourRole::pink},
      Swatch{"brown", theme::ColourRole::brown},
      Swatch{"gray", theme::ColourRole::gray},
      Swatch{"gray2", theme::ColourRole::gray2},
      Swatch{"gray3", theme::ColourRole::gray3},
      Swatch{"gray4", theme::ColourRole::gray4},
      Swatch{"gray5", theme::ColourRole::gray5},
      Swatch{"gray6", theme::ColourRole::gray6}};
  static constexpr std::array macos{
      Swatch{"window_background", theme::ColourRole::window_background},
      Swatch{"under_page_background", theme::ColourRole::under_page_background},
      Swatch{"control_background", theme::ColourRole::control_background},
      Swatch{"text_background", theme::ColourRole::text_background},
      Swatch{"alternating_content_background",
             theme::ColourRole::alternating_content_background},
      Swatch{"alternating_content_background_alternate",
             theme::ColourRole::alternating_content_background_alternate},
      Swatch{"scrubber_textured_background",
             theme::ColourRole::scrubber_textured_background},
      Swatch{"grid", theme::ColourRole::grid},
      Swatch{"header_text", theme::ColourRole::header_text},
      Swatch{"window_frame_text", theme::ColourRole::window_frame_text},
      Swatch{"control", theme::ColourRole::control},
      Swatch{"control_text", theme::ColourRole::control_text},
      Swatch{"disabled_control_text", theme::ColourRole::disabled_control_text},
      Swatch{"selected_control", theme::ColourRole::selected_control},
      Swatch{"selected_control_text", theme::ColourRole::selected_control_text},
      Swatch{"alternate_selected_control_text",
             theme::ColourRole::alternate_selected_control_text},
      Swatch{"selected_text", theme::ColourRole::selected_text},
      Swatch{"selected_text_background",
             theme::ColourRole::selected_text_background},
      Swatch{"selected_menu_item_text",
             theme::ColourRole::selected_menu_item_text},
      Swatch{"selected_content_background",
             theme::ColourRole::selected_content_background},
      Swatch{"unemphasized_selected_content_background",
             theme::ColourRole::unemphasized_selected_content_background},
      Swatch{"unemphasized_selected_text_background",
             theme::ColourRole::unemphasized_selected_text_background},
      Swatch{"unemphasized_selected_text",
             theme::ColourRole::unemphasized_selected_text},
      Swatch{"find_highlight", theme::ColourRole::find_highlight},
      Swatch{"highlight", theme::ColourRole::highlight},
      Swatch{"shadow", theme::ColourRole::shadow}};

  auto operator==(const ColoursPage&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

  static auto card_key(std::string_view role) -> std::string;

  static auto chip_key(std::string_view role, std::string_view variant)
      -> std::string;

  static auto groups() -> std::span<const std::span<const Swatch>>;

  static auto shows(const theme::AppearanceVariants& resolved,
                    const Variant& variant) -> bool;

 private:
  static auto section(std::string_view title, std::span<const Swatch> swatches,
                      const theme::Theme& theme) -> composition::Element;

  static auto card(const Swatch& entry, const theme::Theme& theme)
      -> composition::Element;

  static auto chip(const Swatch& entry, const Variant& variant,
                   theme::Colour colour) -> composition::Element;
};

}
