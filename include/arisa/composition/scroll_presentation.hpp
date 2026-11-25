#pragma once

#include <arisa/composition/axes.hpp>
#include <arisa/composition/point.hpp>

namespace arisa::composition {

struct ScrollPresentation {
  Point offset{};
  float indicators{0.0F};
  Axes indicator_axes{Axes::all};

  friend constexpr auto operator==(const ScrollPresentation&,
                                   const ScrollPresentation&) -> bool = default;
};

}
