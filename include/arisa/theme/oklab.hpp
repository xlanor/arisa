#pragma once

#include <arisa/theme/colour.hpp>

namespace arisa::theme {

class Oklab {
 public:
  constexpr Oklab(float lightness, float a, float b) noexcept
      : lightness_(lightness), a_(a), b_(b) {}

  static auto from(Colour colour) noexcept -> Oklab;

  auto to_colour(float alpha = 1.0F) const noexcept -> Colour;

  constexpr auto lightness() const noexcept -> float { return lightness_; }

  constexpr auto a() const noexcept -> float { return a_; }

  constexpr auto b() const noexcept -> float { return b_; }

  friend constexpr auto operator==(const Oklab&, const Oklab&)
      -> bool = default;

 private:
  float lightness_;
  float a_;
  float b_;
};

}
