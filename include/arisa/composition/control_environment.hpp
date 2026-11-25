#pragma once

#include <arisa/composition/axis.hpp>
#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/menu_action_dismiss_behavior.hpp>
#include <arisa/composition/menu_order.hpp>
#include <arisa/composition/visibility.hpp>

#include <optional>

namespace arisa::composition {

class ControlEnvironment {
 public:
  static const EnvironmentKey<Visibility> labels_visibility;
  static const EnvironmentKey<Visibility> menu_indicator;
  static const EnvironmentKey<MenuOrder> menu_order;
  static const EnvironmentKey<MenuActionDismissBehavior>
      menu_action_dismiss_behavior;
  static const EnvironmentKey<bool> menu_opens_upward;
  static const EnvironmentKey<std::optional<float>> label_icon_to_title_spacing;
  static const EnvironmentKey<std::optional<float>> label_reserved_icon_width;
  static const EnvironmentKey<Visibility> slider_thumb_visibility;
  static const EnvironmentKey<Axis> radio_group_layout;
};

}
