#pragma once

#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/keyboard_shortcut.hpp>

#include <optional>

namespace arisa::composition {

class KeyboardEnvironment {
 public:
  static const EnvironmentKey<std::optional<KeyboardShortcut>> shortcut;
};

}
