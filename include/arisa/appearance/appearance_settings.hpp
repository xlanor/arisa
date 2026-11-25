#pragma once

#include <arisa/composition/navigation_split_view_visibility.hpp>
#include <arisa/provider/haptics/haptic_strength.hpp>
#include <arisa/provider/surface/display_mode.hpp>
#include <arisa/theme/color_scheme.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/dynamic_type_size.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace arisa::appearance {

enum class ColorSchemePreference : std::uint8_t {
  system,
  light,
  dark,
};

struct UiScales {
  static constexpr float handheld_default = 1.25F;
  static constexpr float docked_default = 1.25F;
  static constexpr float desktop_default = 1.0F;
  static constexpr float minimum = 0.75F;
  static constexpr float maximum = 2.0F;

  std::optional<float> handheld;
  std::optional<float> docked;
  std::optional<float> desktop;

  static auto platform_default(provider::DisplayMode mode) noexcept -> float;

  auto resolve(provider::DisplayMode mode) const noexcept -> float;

  auto with(provider::DisplayMode mode, float scale) const noexcept -> UiScales;

  friend auto operator==(const UiScales&, const UiScales&) -> bool = default;
};

struct AppearanceSettings {
  std::string theme{"default"};
  ColorSchemePreference color_scheme{ColorSchemePreference::system};
  std::optional<theme::Colour> accent;
  theme::DynamicTypeSize dynamic_type_size{theme::DynamicTypeSize::large};
  UiScales ui_scale;
  bool reduce_motion{false};
  bool reduce_transparency{false};
  bool increase_contrast{false};
  bool bold_text{false};
  bool button_shapes{false};
  bool differentiate_without_color{false};
  bool dim_flashing_lights{false};
  bool voice_over{false};
  bool switch_control{false};
  bool invert_colors{false};
  bool play_animated_images{true};
  bool prefers_cross_fade_transitions{false};
  bool reduce_highlighting_effects{false};
  bool show_borders{false};
  bool assistive_access{false};
  bool sensory_feedback{true};
  provider::HapticStrength haptic_strength;
  composition::NavigationSplitViewVisibility sidebar{
      composition::NavigationSplitViewVisibility::all};

  auto resolved_scheme() const noexcept -> std::optional<theme::ColorScheme>;

  friend auto operator==(const AppearanceSettings&, const AppearanceSettings&)
      -> bool = default;
};

struct AppearanceOverrides {
  std::optional<std::string> theme;
  std::optional<ColorSchemePreference> color_scheme;
  std::optional<theme::Colour> accent;
  std::optional<theme::DynamicTypeSize> dynamic_type_size;
  UiScales ui_scale;
  std::optional<bool> reduce_motion;
  std::optional<bool> reduce_transparency;
  std::optional<bool> increase_contrast;
  std::optional<bool> bold_text;
  std::optional<bool> button_shapes;
  std::optional<bool> differentiate_without_color;
  std::optional<bool> dim_flashing_lights;
  std::optional<bool> voice_over;
  std::optional<bool> switch_control;
  std::optional<bool> invert_colors;
  std::optional<bool> play_animated_images;
  std::optional<bool> prefers_cross_fade_transitions;
  std::optional<bool> reduce_highlighting_effects;
  std::optional<bool> show_borders;
  std::optional<bool> assistive_access;
  std::optional<bool> sensory_feedback;
  std::optional<provider::HapticStrength> haptic_strength;
  std::optional<composition::NavigationSplitViewVisibility> sidebar;

  auto over(AppearanceSettings base) const -> AppearanceSettings;

  friend auto operator==(const AppearanceOverrides&, const AppearanceOverrides&)
      -> bool = default;
};

}
