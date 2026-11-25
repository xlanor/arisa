#pragma once

#include <arisa/composition/environment_key.hpp>
#include <arisa/theme/typography.hpp>

#include <optional>

namespace arisa::composition {

class TextFont {
 public:
  static const EnvironmentKey<std::optional<theme::Font>> font;
};

}
