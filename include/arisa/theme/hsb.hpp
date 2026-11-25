#pragma once

#include <arisa/theme/colour.hpp>

namespace arisa::theme {

struct Hsb {
  float hue{0.0F};
  float saturation{0.0F};
  float brightness{0.0F};
  float alpha{1.0F};

  static auto from(Colour colour) noexcept -> Hsb;

  auto colour() const noexcept -> Colour;

  friend constexpr auto operator==(const Hsb&, const Hsb&) -> bool = default;
};

}
