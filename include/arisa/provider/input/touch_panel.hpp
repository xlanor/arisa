#pragma once

#include <arisa/provider/input/provider.hpp>
#include <arisa/provider/surface/surface_size.hpp>

namespace arisa::provider {

class TouchPanel {
 public:
  constexpr explicit TouchPanel(SurfaceSize extent) noexcept
      : extent_(extent) {}

  constexpr auto extent() const noexcept -> SurfaceSize { return extent_; }

  auto to_pixels(PixelPoint reported, SurfaceSize framebuffer) const noexcept
      -> PixelPoint;

  auto to_panel(PixelPoint pixels, SurfaceSize framebuffer) const noexcept
      -> PixelPoint;

  friend constexpr auto operator==(const TouchPanel&, const TouchPanel&)
      -> bool = default;

 private:
  auto ratio(SurfaceSize framebuffer) const noexcept -> PixelPoint;

  SurfaceSize extent_;
};

}
