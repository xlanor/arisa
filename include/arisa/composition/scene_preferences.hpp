#pragma once

#include <arisa/composition/identity.hpp>

namespace arisa::composition {

class ScenePreferences {
 public:
  static auto crosses(Identity preference) noexcept -> bool;
};

}
