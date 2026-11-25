#include <arisa/composition/keyboard_environment.hpp>

namespace arisa::composition {

const EnvironmentKey<std::optional<KeyboardShortcut>>
    KeyboardEnvironment::shortcut{"keyboard_shortcut", std::nullopt};

}
