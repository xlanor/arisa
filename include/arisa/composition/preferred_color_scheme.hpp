#pragma once

#include <arisa/composition/preference_key.hpp>
#include <arisa/theme/color_scheme.hpp>

#include <optional>

namespace arisa::composition {

class PreferredColorScheme {
 public:
  static const PreferenceKey<std::optional<theme::ColorScheme>> key;
};

}
