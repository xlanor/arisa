#pragma once

#include <arisa/composition/accessibility_technologies.hpp>
#include <arisa/composition/environment_key.hpp>

namespace arisa::composition {

class AccessibilitySettings {
 public:
  static const EnvironmentKey<bool> differentiate_without_color;
  static const EnvironmentKey<bool> show_button_shapes;
  static const EnvironmentKey<bool> dim_flashing_lights;
  static const EnvironmentKey<AccessibilityTechnologies> technologies;
  static const EnvironmentKey<bool> enabled;
  static const EnvironmentKey<bool> voice_over_enabled;
  static const EnvironmentKey<bool> switch_control_enabled;
  static const EnvironmentKey<bool> invert_colors;
  static const EnvironmentKey<bool> play_animated_images;
  static const EnvironmentKey<bool> prefers_cross_fade_transitions;
  static const EnvironmentKey<bool> reduce_highlighting_effects;
  static const EnvironmentKey<bool> show_borders;
  static const EnvironmentKey<bool> assistive_access_enabled;
  static const EnvironmentKey<bool> large_content_viewer_enabled;
};

}
