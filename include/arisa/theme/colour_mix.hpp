#pragma once

#include <arisa/theme/colour.hpp>
#include <arisa/theme/colour_interpolation.hpp>
#include <arisa/theme/rgb_color_space.hpp>

namespace arisa::theme {

class ColourMix {
 public:
  static auto mix(Colour from, Colour to, float amount,
                  ColourInterpolation space) noexcept -> Colour;

  static auto rgb(RGBColorSpace space, float red, float green, float blue,
                  float opacity = 1.0F) noexcept -> Colour;

  static auto grey(RGBColorSpace space, float white,
                   float opacity = 1.0F) noexcept -> Colour;

  static auto exposed(Colour colour, float stops) noexcept -> Colour;

  static auto encoded(float linear) noexcept -> float;

  static auto decoded(float encoded) noexcept -> float;
};

}
