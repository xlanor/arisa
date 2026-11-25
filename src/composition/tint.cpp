#include <arisa/composition/tint.hpp>

namespace arisa::composition {

const EnvironmentKey<std::optional<graphics::ShapeStyle>> Tint::key{
    "tint", std::nullopt};

}
