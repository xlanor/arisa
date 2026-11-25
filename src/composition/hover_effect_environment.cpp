#include <arisa/composition/hover_effect_environment.hpp>

namespace arisa::composition {

const EnvironmentKey<bool> HoverEffectEnvironment::is_hover_effect_enabled{
    "is_hover_effect_enabled", true};

const EnvironmentKey<std::optional<CustomHoverEffect>>
    HoverEffectEnvironment::default_hover_effect{"default_hover_effect",
                                                 std::nullopt};

}
