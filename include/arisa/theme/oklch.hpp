#pragma once

#include <arisa/theme/colour.hpp>

namespace arisa::theme {

class Oklch {
 public:
  constexpr Oklch(float lightness, float chroma, float hue) noexcept
      : lightness_(lightness), chroma_(chroma), hue_(hue) {}

  static auto from(Colour colour) noexcept -> Oklch;

  auto to_colour(float alpha = 1.0F) const noexcept -> Colour;

  auto in_gamut() const noexcept -> bool;

  constexpr auto lightness() const noexcept -> float { return lightness_; }

  constexpr auto chroma() const noexcept -> float { return chroma_; }

  constexpr auto hue() const noexcept -> float { return hue_; }

  auto with_lightness(float value) const noexcept -> Oklch;

  auto with_chroma(float value) const noexcept -> Oklch;

  friend constexpr auto operator==(const Oklch&, const Oklch&)
      -> bool = default;

 private:
  struct Linear {
    float red;
    float green;
    float blue;
  };

  static auto decode(float channel) noexcept -> float;

  static auto encode(float channel) noexcept -> float;

  auto linear() const noexcept -> Linear;

  float lightness_;
  float chroma_;
  float hue_;
};

}
