#pragma once

#include <arisa/provider/surface/surface_scale.hpp>
#include <arisa/provider/surface/surface_size.hpp>

namespace arisa::layout {

struct Viewport {
  provider::SurfaceSize pixels{};
  provider::SurfaceScale scale{provider::SurfaceScale::unit()};

  constexpr auto width() const noexcept -> float {
    return static_cast<float>(pixels.width.value()) / scale.value();
  }

  constexpr auto height() const noexcept -> float {
    return static_cast<float>(pixels.height.value()) / scale.value();
  }

  friend constexpr auto operator==(const Viewport&, const Viewport&)
      -> bool = default;
};

}
