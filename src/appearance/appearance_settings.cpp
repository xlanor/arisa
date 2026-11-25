#include <arisa/appearance/appearance_settings.hpp>

#include <algorithm>

namespace arisa::appearance {

auto UiScales::platform_default(provider::DisplayMode mode) noexcept -> float {
  switch (mode) {
    case provider::DisplayMode::handheld:
      return handheld_default;
    case provider::DisplayMode::docked:
      return docked_default;
    case provider::DisplayMode::desktop:
      break;
  }
  return desktop_default;
}

auto UiScales::resolve(provider::DisplayMode mode) const noexcept -> float {
  std::optional<float> chosen;
  switch (mode) {
    case provider::DisplayMode::handheld:
      chosen = handheld;
      break;
    case provider::DisplayMode::docked:
      chosen = docked;
      break;
    case provider::DisplayMode::desktop:
      chosen = desktop;
      break;
  }
  return std::clamp(chosen.value_or(platform_default(mode)), minimum, maximum);
}

auto UiScales::with(provider::DisplayMode mode, float scale) const noexcept
    -> UiScales {
  UiScales changed = *this;
  const float clamped = std::clamp(scale, minimum, maximum);
  switch (mode) {
    case provider::DisplayMode::handheld:
      changed.handheld = clamped;
      break;
    case provider::DisplayMode::docked:
      changed.docked = clamped;
      break;
    case provider::DisplayMode::desktop:
      changed.desktop = clamped;
      break;
  }
  return changed;
}

auto AppearanceSettings::resolved_scheme() const noexcept
    -> std::optional<theme::ColorScheme> {
  switch (color_scheme) {
    case ColorSchemePreference::light:
      return theme::ColorScheme::light;
    case ColorSchemePreference::dark:
      return theme::ColorScheme::dark;
    case ColorSchemePreference::system:
      break;
  }
  return std::nullopt;
}

auto AppearanceOverrides::over(AppearanceSettings base) const
    -> AppearanceSettings {
  if (theme.has_value()) {
    base.theme = *theme;
  }
  if (color_scheme.has_value()) {
    base.color_scheme = *color_scheme;
  }
  if (accent.has_value()) {
    base.accent = accent;
  }
  if (dynamic_type_size.has_value()) {
    base.dynamic_type_size = *dynamic_type_size;
  }
  if (ui_scale.handheld.has_value()) {
    base.ui_scale.handheld = ui_scale.handheld;
  }
  if (ui_scale.docked.has_value()) {
    base.ui_scale.docked = ui_scale.docked;
  }
  if (ui_scale.desktop.has_value()) {
    base.ui_scale.desktop = ui_scale.desktop;
  }
  if (reduce_motion.has_value()) {
    base.reduce_motion = *reduce_motion;
  }
  if (reduce_transparency.has_value()) {
    base.reduce_transparency = *reduce_transparency;
  }
  if (increase_contrast.has_value()) {
    base.increase_contrast = *increase_contrast;
  }
  if (bold_text.has_value()) {
    base.bold_text = *bold_text;
  }
  if (button_shapes.has_value()) {
    base.button_shapes = *button_shapes;
  }
  if (differentiate_without_color.has_value()) {
    base.differentiate_without_color = *differentiate_without_color;
  }
  if (dim_flashing_lights.has_value()) {
    base.dim_flashing_lights = *dim_flashing_lights;
  }
  if (voice_over.has_value()) {
    base.voice_over = *voice_over;
  }
  if (switch_control.has_value()) {
    base.switch_control = *switch_control;
  }
  if (invert_colors.has_value()) {
    base.invert_colors = *invert_colors;
  }
  if (play_animated_images.has_value()) {
    base.play_animated_images = *play_animated_images;
  }
  if (prefers_cross_fade_transitions.has_value()) {
    base.prefers_cross_fade_transitions = *prefers_cross_fade_transitions;
  }
  if (reduce_highlighting_effects.has_value()) {
    base.reduce_highlighting_effects = *reduce_highlighting_effects;
  }
  if (show_borders.has_value()) {
    base.show_borders = *show_borders;
  }
  if (assistive_access.has_value()) {
    base.assistive_access = *assistive_access;
  }
  if (sensory_feedback.has_value()) {
    base.sensory_feedback = *sensory_feedback;
  }
  if (haptic_strength.has_value()) {
    base.haptic_strength = *haptic_strength;
  }
  if (sidebar.has_value()) {
    base.sidebar = *sidebar;
  }
  return base;
}

}
