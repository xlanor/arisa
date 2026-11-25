#pragma once

#include <arisa/composition/environment_key.hpp>

namespace arisa::composition {

struct FocusEnvironment {
  static const EnvironmentKey<bool> is_focused;
};

}
