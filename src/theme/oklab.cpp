#include <arisa/theme/oklab.hpp>
#include <arisa/theme/oklch.hpp>

#include <cmath>
#include <numbers>

namespace arisa::theme {

auto Oklab::from(Colour colour) noexcept -> Oklab {
  const Oklch polar = Oklch::from(colour);
  const float radians = polar.hue() * std::numbers::pi_v<float> / 180.0F;
  return Oklab{polar.lightness(), polar.chroma() * std::cos(radians),
               polar.chroma() * std::sin(radians)};
}

auto Oklab::to_colour(float alpha) const noexcept -> Colour {
  const float chroma = std::hypot(a_, b_);
  float hue = std::atan2(b_, a_) * 180.0F / std::numbers::pi_v<float>;
  if (hue < 0.0F) {
    hue += 360.0F;
  }
  return Oklch{lightness_, chroma, chroma < 1.0e-5F ? 0.0F : hue}.to_colour(
      alpha);
}

}
