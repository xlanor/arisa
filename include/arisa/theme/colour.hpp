#pragma once

#include <cmath>
#include <cstdint>

namespace arisa::theme {

struct Colour {
  float red{0.0F};
  float green{0.0F};
  float blue{0.0F};
  float alpha{1.0F};

  static const Colour clear;

  static constexpr auto hex(std::uint32_t rgb, float alpha = 1.0F) noexcept
      -> Colour {
    return Colour{.red = static_cast<float>((rgb >> 16U) & 0xffU) / 255.0F,
                  .green = static_cast<float>((rgb >> 8U) & 0xffU) / 255.0F,
                  .blue = static_cast<float>(rgb & 0xffU) / 255.0F,
                  .alpha = alpha};
  }

  static auto linearised(float component) noexcept -> float {
    return component <= 0.04045F
               ? component / 12.92F
               : std::pow((component + 0.055F) / 1.055F, 2.4F);
  }

  auto linear_red() const noexcept -> float { return linearised(red); }

  auto linear_green() const noexcept -> float { return linearised(green); }

  auto linear_blue() const noexcept -> float { return linearised(blue); }

  constexpr auto with_alpha(float value) const noexcept -> Colour {
    return Colour{.red = red, .green = green, .blue = blue, .alpha = value};
  }

  friend constexpr auto operator==(const Colour&, const Colour&)
      -> bool = default;
};

inline constexpr Colour Colour::clear{
    .red = 0.0F, .green = 0.0F, .blue = 0.0F, .alpha = 0.0F};

}
