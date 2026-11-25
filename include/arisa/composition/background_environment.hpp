#pragma once

#include <arisa/composition/background_prominence.hpp>
#include <arisa/composition/environment_key.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/motion/content_transition.hpp>
#include <arisa/theme/material.hpp>

#include <optional>

namespace arisa::composition {

class BackgroundEnvironment {
 public:
  static const EnvironmentKey<std::optional<graphics::ShapeStyle>> style;
  static const EnvironmentKey<std::optional<theme::Material>> material;
  static const EnvironmentKey<BackgroundProminence> prominence;
  static const EnvironmentKey<bool> content_transition_adds_drawing_group;
  static const EnvironmentKey<motion::ContentTransition> content_transition;
};

}
