#pragma once

#include <arisa/composition/environment_key.hpp>
#include <arisa/theme/space.hpp>

namespace arisa::composition {

class SafeArea {
 public:
  static const EnvironmentKey<theme::Edges> insets;
};

}
