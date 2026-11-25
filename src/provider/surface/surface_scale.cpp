#include <arisa/provider/surface/surface_scale.hpp>

#include <algorithm>
#include <cmath>

namespace arisa::provider {

auto SurfaceScale::create(float value) noexcept -> std::optional<SurfaceScale> {
  if (!std::isfinite(value) || value <= 0.0F) {
    return std::nullopt;
  }
  return SurfaceScale{value};
}

auto SurfaceScale::for_pixels(SurfaceSize size) noexcept -> SurfaceScale {
  if (size.empty()) {
    return unit();
  }
  const float horizontal =
      static_cast<float>(size.width.value()) / reference_width;
  const float vertical =
      static_cast<float>(size.height.value()) / reference_height;
  return SurfaceScale{std::max(std::min(horizontal, vertical), minimum)};
}

}
