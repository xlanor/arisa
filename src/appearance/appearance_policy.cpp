#include <arisa/appearance/appearance_policy.hpp>

namespace arisa::appearance {

auto AppearancePolicy::enforce(AppearanceSettings wanted) const
    -> AppearanceSettings {
  if (locks.contains(AppearanceField::theme)) {
    wanted.theme = defaults.theme;
  }
  if (locks.contains(AppearanceField::color_scheme)) {
    wanted.color_scheme = defaults.color_scheme;
  }
  if (locks.contains(AppearanceField::accent)) {
    wanted.accent = defaults.accent;
  }
  if (locks.contains(AppearanceField::dynamic_type_size)) {
    wanted.dynamic_type_size = defaults.dynamic_type_size;
  }
  if (locks.contains(AppearanceField::ui_scale)) {
    wanted.ui_scale = defaults.ui_scale;
  }
  if (locks.contains(AppearanceField::reduce_motion)) {
    wanted.reduce_motion = defaults.reduce_motion;
  }
  if (locks.contains(AppearanceField::reduce_transparency)) {
    wanted.reduce_transparency = defaults.reduce_transparency;
  }
  if (locks.contains(AppearanceField::increase_contrast)) {
    wanted.increase_contrast = defaults.increase_contrast;
  }
  if (locks.contains(AppearanceField::bold_text)) {
    wanted.bold_text = defaults.bold_text;
  }
  if (locks.contains(AppearanceField::button_shapes)) {
    wanted.button_shapes = defaults.button_shapes;
  }
  if (locks.contains(AppearanceField::differentiate_without_color)) {
    wanted.differentiate_without_color = defaults.differentiate_without_color;
  }
  if (locks.contains(AppearanceField::dim_flashing_lights)) {
    wanted.dim_flashing_lights = defaults.dim_flashing_lights;
  }
  if (locks.contains(AppearanceField::voice_over)) {
    wanted.voice_over = defaults.voice_over;
  }
  if (locks.contains(AppearanceField::switch_control)) {
    wanted.switch_control = defaults.switch_control;
  }
  if (locks.contains(AppearanceField::invert_colors)) {
    wanted.invert_colors = defaults.invert_colors;
  }
  if (locks.contains(AppearanceField::play_animated_images)) {
    wanted.play_animated_images = defaults.play_animated_images;
  }
  if (locks.contains(AppearanceField::prefers_cross_fade_transitions)) {
    wanted.prefers_cross_fade_transitions =
        defaults.prefers_cross_fade_transitions;
  }
  if (locks.contains(AppearanceField::reduce_highlighting_effects)) {
    wanted.reduce_highlighting_effects = defaults.reduce_highlighting_effects;
  }
  if (locks.contains(AppearanceField::show_borders)) {
    wanted.show_borders = defaults.show_borders;
  }
  if (locks.contains(AppearanceField::assistive_access)) {
    wanted.assistive_access = defaults.assistive_access;
  }
  if (locks.contains(AppearanceField::sensory_feedback)) {
    wanted.sensory_feedback = defaults.sensory_feedback;
  }
  if (locks.contains(AppearanceField::haptic_strength)) {
    wanted.haptic_strength = defaults.haptic_strength;
  }
  if (locks.contains(AppearanceField::sidebar)) {
    wanted.sidebar = defaults.sidebar;
  }
  return wanted;
}

}
