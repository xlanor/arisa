#include <arisa/theme/oklch.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace arisa::theme {

auto Oklch::decode(float channel) noexcept -> float {
  if (channel <= 0.04045F) {
    return channel / 12.92F;
  }
  return std::pow((channel + 0.055F) / 1.055F, 2.4F);
}

auto Oklch::encode(float channel) noexcept -> float {
  if (channel <= 0.0031308F) {
    return channel * 12.92F;
  }
  return (1.055F * std::pow(channel, 1.0F / 2.4F)) - 0.055F;
}

auto Oklch::from(Colour colour) noexcept -> Oklch {
  const float red = decode(colour.red);
  const float green = decode(colour.green);
  const float blue = decode(colour.blue);
  const float long_cone = std::cbrt(
      (0.4122214708F * red) + (0.5363325363F * green) + (0.0514459929F * blue));
  const float medium_cone = std::cbrt(
      (0.2119034982F * red) + (0.6806995451F * green) + (0.1073969566F * blue));
  const float short_cone = std::cbrt(
      (0.0883024619F * red) + (0.2817188376F * green) + (0.6299787005F * blue));
  const float lightness = (0.2104542553F * long_cone) +
                          (0.7936177850F * medium_cone) -
                          (0.0040720468F * short_cone);
  const float a = (1.9779984951F * long_cone) - (2.4285922050F * medium_cone) +
                  (0.4505937099F * short_cone);
  const float b = (0.0259040371F * long_cone) + (0.7827717662F * medium_cone) -
                  (0.8086757660F * short_cone);
  const float chroma = std::hypot(a, b);
  float hue = std::atan2(b, a) * 180.0F / std::numbers::pi_v<float>;
  if (hue < 0.0F) {
    hue += 360.0F;
  }
  return Oklch{lightness, chroma, chroma < 1.0e-5F ? 0.0F : hue};
}

auto Oklch::linear() const noexcept -> Linear {
  const float radians = hue_ * std::numbers::pi_v<float> / 180.0F;
  const float a = chroma_ * std::cos(radians);
  const float b = chroma_ * std::sin(radians);
  const float long_root =
      lightness_ + (0.3963377774F * a) + (0.2158037573F * b);
  const float medium_root =
      lightness_ - (0.1055613458F * a) - (0.0638541728F * b);
  const float short_root =
      lightness_ - (0.0894841775F * a) - (1.2914855480F * b);
  const float long_cone = long_root * long_root * long_root;
  const float medium_cone = medium_root * medium_root * medium_root;
  const float short_cone = short_root * short_root * short_root;
  return Linear{
      .red = (4.0767416621F * long_cone) - (3.3077115913F * medium_cone) +
             (0.2309699292F * short_cone),
      .green = (-1.2684380046F * long_cone) + (2.6097574011F * medium_cone) -
               (0.3413193965F * short_cone),
      .blue = (-0.0041960863F * long_cone) - (0.7034186147F * medium_cone) +
              (1.7076147010F * short_cone)};
}

auto Oklch::in_gamut() const noexcept -> bool {
  constexpr float tolerance = 1.0e-4F;
  const Linear value = linear();
  return std::ranges::all_of(
      std::array{value.red, value.green, value.blue}, [](float channel) {
        return channel >= -tolerance && channel <= 1.0F + tolerance;
      });
}

auto Oklch::with_lightness(float value) const noexcept -> Oklch {
  return Oklch{std::clamp(value, 0.0F, 1.0F), chroma_, hue_};
}

auto Oklch::with_chroma(float value) const noexcept -> Oklch {
  return Oklch{lightness_, std::max(value, 0.0F), hue_};
}

auto Oklch::to_colour(float alpha) const noexcept -> Colour {
  constexpr int steps = 24;
  Oklch mapped = *this;
  if (!mapped.in_gamut()) {
    float low = 0.0F;
    float high = chroma_;
    for (int step = 0; step < steps; ++step) {
      const float middle = (low + high) * 0.5F;
      if (with_chroma(middle).in_gamut()) {
        low = middle;
      } else {
        high = middle;
      }
    }
    mapped = with_chroma(low);
  }
  const Linear value = mapped.linear();
  return Colour{.red = encode(std::clamp(value.red, 0.0F, 1.0F)),
                .green = encode(std::clamp(value.green, 0.0F, 1.0F)),
                .blue = encode(std::clamp(value.blue, 0.0F, 1.0F)),
                .alpha = alpha};
}

}
