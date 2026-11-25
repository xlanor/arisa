#pragma once

#include <arisa/theme/color_scheme.hpp>

namespace arisa::theme {

template <class T>
struct Adaptive {
  T light;
  T dark;

  constexpr auto resolve(ColorScheme scheme) const noexcept -> const T& {
    return scheme == ColorScheme::dark ? dark : light;
  }

  friend constexpr auto operator==(const Adaptive&, const Adaptive&)
      -> bool = default;
};

}
