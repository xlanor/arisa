#pragma once

#include <arisa/composition/environment_key.hpp>

namespace arisa::composition {

class FocusEffect {
 public:
  static const EnvironmentKey<bool> is_focus_effect_enabled;
};

}
