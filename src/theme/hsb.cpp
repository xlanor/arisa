#include <arisa/theme/hsb.hpp>

#include <algorithm>
#include <cmath>

namespace arisa::theme {

auto Hsb::from(Colour colour) noexcept -> Hsb {
  const float red = std::clamp(colour.red, 0.0F, 1.0F);
  const float green = std::clamp(colour.green, 0.0F, 1.0F);
  const float blue = std::clamp(colour.blue, 0.0F, 1.0F);
  const float highest = std::max({red, green, blue});
  const float lowest = std::min({red, green, blue});
  const float range = highest - lowest;
  float hue = 0.0F;
  if (range > 0.0F) {
    if (highest == red) {
      hue = (green - blue) / range;
    } else if (highest == green) {
      hue = 2.0F + ((blue - red) / range);
    } else {
      hue = 4.0F + ((red - green) / range);
    }
    hue *= 60.0F;
    if (hue < 0.0F) {
      hue += 360.0F;
    }
  }
  return Hsb{.hue = hue,
             .saturation = highest > 0.0F ? range / highest : 0.0F,
             .brightness = highest,
             .alpha = std::clamp(colour.alpha, 0.0F, 1.0F)};
}

auto Hsb::colour() const noexcept -> Colour {
  const float value = std::clamp(brightness, 0.0F, 1.0F);
  const float chroma = value * std::clamp(saturation, 0.0F, 1.0F);
  const float sector =
      std::fmod(std::fmod(hue, 360.0F) + 360.0F, 360.0F) / 60.0F;
  const float second =
      chroma * (1.0F - std::fabs(std::fmod(sector, 2.0F) - 1.0F));
  float red = 0.0F;
  float green = 0.0F;
  float blue = 0.0F;
  if (sector < 1.0F) {
    red = chroma;
    green = second;
  } else if (sector < 2.0F) {
    red = second;
    green = chroma;
  } else if (sector < 3.0F) {
    green = chroma;
    blue = second;
  } else if (sector < 4.0F) {
    green = second;
    blue = chroma;
  } else if (sector < 5.0F) {
    red = second;
    blue = chroma;
  } else {
    red = chroma;
    blue = second;
  }
  const float lift = value - chroma;
  return Colour{.red = red + lift,
                .green = green + lift,
                .blue = blue + lift,
                .alpha = std::clamp(alpha, 0.0F, 1.0F)};
}

}
