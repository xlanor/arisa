#include <arisa/provider/input/touch_panel.hpp>

#include <algorithm>

namespace arisa::provider {

auto TouchPanel::ratio(SurfaceSize framebuffer) const noexcept -> PixelPoint {
  if (extent_.empty() || framebuffer.empty()) {
    return PixelPoint{.x = 1.0F, .y = 1.0F};
  }
  return PixelPoint{.x = static_cast<float>(framebuffer.width.value()) /
                         static_cast<float>(extent_.width.value()),
                    .y = static_cast<float>(framebuffer.height.value()) /
                         static_cast<float>(extent_.height.value())};
}

auto TouchPanel::to_pixels(PixelPoint reported,
                           SurfaceSize framebuffer) const noexcept
    -> PixelPoint {
  const PixelPoint factor = ratio(framebuffer);
  const auto width = static_cast<float>(extent_.width.value());
  const auto height = static_cast<float>(extent_.height.value());
  const float x =
      extent_.empty() ? reported.x : std::clamp(reported.x, 0.0F, width);
  const float y =
      extent_.empty() ? reported.y : std::clamp(reported.y, 0.0F, height);
  return PixelPoint{.x = x * factor.x, .y = y * factor.y};
}

auto TouchPanel::to_panel(PixelPoint pixels,
                          SurfaceSize framebuffer) const noexcept
    -> PixelPoint {
  const PixelPoint factor = ratio(framebuffer);
  return PixelPoint{.x = pixels.x / factor.x, .y = pixels.y / factor.y};
}

}
