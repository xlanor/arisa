#pragma once

#include <arisa/composition/environment_key.hpp>

namespace arisa::composition {

class Enablement {
 public:
  static const EnvironmentKey<bool> is_enabled;
};

}
