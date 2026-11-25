#pragma once

#include <arisa/theme/color_scheme.hpp>
#include <arisa/theme/colour.hpp>

namespace arisa::theme {

struct Seed {
  Colour accent{Colour::hex(0x007AFF)};
  ColorScheme scheme{ColorScheme::light};

  friend constexpr auto operator==(const Seed&, const Seed&) -> bool = default;
};

}
