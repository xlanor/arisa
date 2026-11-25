#pragma once

#include <arisa/theme/colour.hpp>

namespace arisa::theme {

struct ResolvedHDR {
  float red{0.0F};
  float green{0.0F};
  float blue{0.0F};
  float opacity{1.0F};
  float headroom{1.0F};

  static constexpr auto from(Colour resolved, float headroom = 1.0F) noexcept
      -> ResolvedHDR {
    return ResolvedHDR{.red = resolved.red,
                       .green = resolved.green,
                       .blue = resolved.blue,
                       .opacity = resolved.alpha,
                       .headroom = headroom};
  }

  auto linear_red() const noexcept -> float { return Colour::linearised(red); }

  auto linear_green() const noexcept -> float {
    return Colour::linearised(green);
  }

  auto linear_blue() const noexcept -> float {
    return Colour::linearised(blue);
  }

  constexpr auto resolved() const noexcept -> Colour {
    return Colour{.red = red, .green = green, .blue = blue, .alpha = opacity};
  }

  friend constexpr auto operator==(const ResolvedHDR&, const ResolvedHDR&)
      -> bool = default;
};

}
