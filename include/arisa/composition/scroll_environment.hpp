#pragma once

#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/scroll_bounce_behavior.hpp>
#include <arisa/composition/scroll_dismisses_keyboard_mode.hpp>
#include <arisa/composition/scroll_edge_effect_style.hpp>
#include <arisa/composition/scroll_indicator_visibility.hpp>
#include <arisa/composition/visibility.hpp>

namespace arisa::composition {

class ScrollEnvironment {
 public:
  static const EnvironmentKey<Visibility> content_background;
  static const EnvironmentKey<bool> is_scroll_enabled;
  static const EnvironmentKey<ScrollIndicatorVisibility>
      horizontal_scroll_indicator_visibility;
  static const EnvironmentKey<ScrollIndicatorVisibility>
      vertical_scroll_indicator_visibility;
  static const EnvironmentKey<ScrollBounceBehavior>
      horizontal_scroll_bounce_behavior;
  static const EnvironmentKey<ScrollBounceBehavior>
      vertical_scroll_bounce_behavior;
  static const EnvironmentKey<ScrollDismissesKeyboardMode>
      scroll_dismisses_keyboard_mode;
  static const EnvironmentKey<ScrollEdgeEffects> scroll_edge_effects;
};

}
