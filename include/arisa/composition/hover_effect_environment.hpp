#pragma once

#include <arisa/composition/custom_hover_effect.hpp>
#include <arisa/composition/environment_key.hpp>

#include <optional>

namespace arisa::composition {

class HoverEffectEnvironment {
 public:
  static const EnvironmentKey<bool> is_hover_effect_enabled;
  static const EnvironmentKey<std::optional<CustomHoverEffect>>
      default_hover_effect;
};

}
