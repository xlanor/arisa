#include <arisa/theme/colour_mix.hpp>
#include <arisa/theme/oklab.hpp>
#include <arisa/theme/oklch.hpp>

#include <cmath>

namespace arisa::theme {

auto ColourMix::encoded(float linear) noexcept -> float {
  const float magnitude = std::abs(linear);
  const float curve =
      magnitude <= 0.0031308F
          ? magnitude * 12.92F
          : (1.055F * std::pow(magnitude, 1.0F / 2.4F)) - 0.055F;
  return std::copysign(curve, linear);
}

auto ColourMix::decoded(float encoded) noexcept -> float {
  const float magnitude = std::abs(encoded);
  const float curve = magnitude <= 0.04045F
                          ? magnitude / 12.92F
                          : std::pow((magnitude + 0.055F) / 1.055F, 2.4F);
  return std::copysign(curve, encoded);
}

auto ColourMix::rgb(RGBColorSpace space, float red, float green, float blue,
                    float opacity) noexcept -> Colour {
  switch (space) {
    case RGBColorSpace::srgb:
      break;
    case RGBColorSpace::srgb_linear:
      return Colour{.red = encoded(red),
                    .green = encoded(green),
                    .blue = encoded(blue),
                    .alpha = opacity};
    case RGBColorSpace::display_p3: {
      const float r = decoded(red);
      const float g = decoded(green);
      const float b = decoded(blue);
      return Colour{.red = encoded((1.2249401F * r) - (0.2249404F * g)),
                    .green = encoded((-0.0420569F * r) + (1.0420571F * g)),
                    .blue = encoded((-0.0196376F * r) - (0.0786361F * g) +
                                    (1.0982735F * b)),
                    .alpha = opacity};
    }
  }
  return Colour{.red = red, .green = green, .blue = blue, .alpha = opacity};
}

auto ColourMix::grey(RGBColorSpace space, float white, float opacity) noexcept
    -> Colour {
  return rgb(space, white, white, white, opacity);
}

auto ColourMix::exposed(Colour colour, float stops) noexcept -> Colour {
  if (stops == 0.0F) {
    return colour;
  }
  const float gain = std::exp2(stops);
  return Colour{.red = encoded(decoded(colour.red) * gain),
                .green = encoded(decoded(colour.green) * gain),
                .blue = encoded(decoded(colour.blue) * gain),
                .alpha = colour.alpha};
}

auto ColourMix::mix(Colour from, Colour to, float amount,
                    ColourInterpolation space) noexcept -> Colour {
  const float alpha = from.alpha + ((to.alpha - from.alpha) * amount);
  switch (space) {
    case ColourInterpolation::device:
      return Colour{.red = from.red + ((to.red - from.red) * amount),
                    .green = from.green + ((to.green - from.green) * amount),
                    .blue = from.blue + ((to.blue - from.blue) * amount),
                    .alpha = alpha};
    case ColourInterpolation::linear: {
      const auto blend = [amount](float first, float second) {
        const float a = decoded(first);
        return encoded(a + ((decoded(second) - a) * amount));
      };
      return Colour{.red = blend(from.red, to.red),
                    .green = blend(from.green, to.green),
                    .blue = blend(from.blue, to.blue),
                    .alpha = alpha};
    }
    case ColourInterpolation::perceptual: {
      const Oklab start = Oklab::from(from);
      const Oklab end = Oklab::from(to);
      return Oklab{
          start.lightness() + ((end.lightness() - start.lightness()) * amount),
          start.a() + ((end.a() - start.a()) * amount),
          start.b() + ((end.b() - start.b()) * amount)}
          .to_colour(alpha);
    }
    case ColourInterpolation::oklch: {
      const Oklch start = Oklch::from(from);
      const Oklch end = Oklch::from(to);
      constexpr float turn = 360.0F;
      float delta = end.hue() - start.hue();
      if (delta > turn * 0.5F) {
        delta -= turn;
      } else if (delta < -turn * 0.5F) {
        delta += turn;
      }
      const float hue = std::fmod(start.hue() + (delta * amount) + turn, turn);
      return Oklch{
          start.lightness() + ((end.lightness() - start.lightness()) * amount),
          start.chroma() + ((end.chroma() - start.chroma()) * amount), hue}
          .to_colour(alpha);
    }
  }
  return from;
}

}
