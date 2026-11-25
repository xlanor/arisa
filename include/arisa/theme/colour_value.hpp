#pragma once

#include <arisa/theme/colour.hpp>
#include <arisa/theme/colour_interpolation.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>

namespace arisa::theme {

enum class ColourRole : std::uint8_t {
  label,
  secondary_label,
  tertiary_label,
  quaternary_label,
  quinary_label,
  placeholder_text,
  link,
  text,
  header_text,
  window_frame_text,
  control,
  control_text,
  disabled_control_text,
  selected_control,
  selected_control_text,
  alternate_selected_control_text,
  selected_text,
  selected_menu_item_text,
  unemphasized_selected_text,
  dark_text,
  light_text,
  on_accent,
  system_background,
  secondary_system_background,
  tertiary_system_background,
  system_grouped_background,
  secondary_system_grouped_background,
  tertiary_system_grouped_background,
  window_background,
  under_page_background,
  control_background,
  text_background,
  alternating_content_background,
  alternating_content_background_alternate,
  scrubber_textured_background,
  system_fill,
  secondary_system_fill,
  tertiary_system_fill,
  quaternary_system_fill,
  quinary_system_fill,
  separator,
  opaque_separator,
  grid,
  highlight,
  shadow,
  scrim,
  accent,
  accent_pressed,
  keyboard_focus_indicator,
  selected_content_background,
  unemphasized_selected_content_background,
  selected_text_background,
  unemphasized_selected_text_background,
  find_highlight,
  red,
  orange,
  yellow,
  green,
  mint,
  teal,
  cyan,
  blue,
  indigo,
  purple,
  pink,
  brown,
  gray,
  gray2,
  gray3,
  gray4,
  gray5,
  gray6,
  background = system_grouped_background,
  surface = secondary_system_background,
  surface_raised = secondary_system_grouped_background,
  primary = label,
  text_primary = label,
  text_secondary = secondary_label,
  focus_ring = keyboard_focus_indicator,
  destructive = red,
  success = green,
  warning = orange,
  tint = accent,
  accent_color = accent,
  control_accent = accent,
  selection = selected_content_background,
  placeholder = placeholder_text,
  foreground = label,
  secondary = secondary_label,
  tertiary = tertiary_label,
  quaternary = quaternary_label,
  quinary = quinary_label,
};

inline constexpr std::size_t colour_role_count = 72;

class ColourValue {
 public:
  using Leaf = std::variant<ColourRole, Colour>;

  struct Step {
    Leaf colour{ColourRole::label};
    float amount{0.0F};
    ColourInterpolation space{ColourInterpolation::perceptual};

    friend constexpr auto operator==(const Step&, const Step&)
        -> bool = default;
  };

  static constexpr std::size_t maximum_steps = 2;

  constexpr ColourValue(ColourRole role) noexcept : value_(role) {}

  constexpr ColourValue(Colour colour) noexcept : value_(colour) {}

  constexpr auto role() const noexcept -> const ColourRole* {
    return std::get_if<ColourRole>(&value_);
  }

  constexpr auto literal() const noexcept -> const Colour* {
    return std::get_if<Colour>(&value_);
  }

  constexpr auto leaf() const noexcept -> const Leaf& { return value_; }

  constexpr auto opacity(float amount) const noexcept -> ColourValue {
    ColourValue faded = *this;
    faded.opacity_ = opacity_ * amount;
    return faded;
  }

  constexpr auto opacity() const noexcept -> float { return opacity_; }

  auto mix(ColourValue other, float amount,
           ColourInterpolation space =
               ColourInterpolation::perceptual) const noexcept -> ColourValue;

  constexpr auto steps() const noexcept -> std::span<const Step> {
    return std::span<const Step>{steps_}.first(step_count_);
  }

  constexpr auto headroom(float value) const noexcept -> ColourValue {
    ColourValue lifted = *this;
    lifted.headroom_ = value;
    return lifted;
  }

  constexpr auto headroom() const noexcept -> float { return headroom_; }

  constexpr auto exposure_adjust(float stops) const noexcept -> ColourValue {
    ColourValue exposed = *this;
    exposed.exposure_ = exposure_ + stops;
    return exposed;
  }

  constexpr auto exposure() const noexcept -> float { return exposure_; }

  auto encoded() const -> std::string;

  static auto decoded(std::string_view text) -> std::optional<ColourValue>;

  static auto content_type() -> std::string;

  friend constexpr auto operator==(const ColourValue&, const ColourValue&)
      -> bool = default;

 private:
  Leaf value_;
  float opacity_{1.0F};
  std::array<Step, maximum_steps> steps_{};
  std::uint8_t step_count_{0};
  float headroom_{1.0F};
  float exposure_{0.0F};
};

using Fill = ColourValue;

}
