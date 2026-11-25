#pragma once

#include <arisa/theme/adaptive.hpp>
#include <arisa/theme/colour.hpp>

namespace arisa::theme {

struct AppearanceVariants {
  Colour light;
  Colour dark;
  Colour dark_elevated;
  Colour high_contrast_light;
  Colour high_contrast_dark;
  Colour high_contrast_dark_elevated;

  constexpr auto adaptive() const noexcept -> Adaptive<Colour> {
    return Adaptive<Colour>{light, dark_elevated};
  }

  friend constexpr auto operator==(const AppearanceVariants&,
                                   const AppearanceVariants&) -> bool = default;
};

}
