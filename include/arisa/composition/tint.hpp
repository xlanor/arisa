#pragma once

#include <arisa/composition/environment_key.hpp>
#include <arisa/graphics/shape_style.hpp>

#include <optional>

namespace arisa::composition {

class Tint {
 public:
  static const EnvironmentKey<std::optional<graphics::ShapeStyle>> key;
};

}
