#pragma once

#include <arisa/provider/gpu/texture.hpp>

#include <algorithm>
#include <array>

namespace arisa::provider {

class YuvConversion {
 public:
  using Packed = std::array<float, 4>;
  using Rgb = std::array<float, 3>;

  static constexpr float chroma_zero = 128.0F / 255.0F;
  static constexpr float limited_black = 16.0F / 255.0F;
  static constexpr float limited_luma_span = 219.0F;
  static constexpr float limited_chroma_span = 224.0F;
  static constexpr float code_span = 255.0F;

  constexpr YuvConversion() noexcept = default;

  constexpr YuvConversion(ColourMatrix matrix, ColourRange range) noexcept
      : matrix_(matrix), range_(range) {}

  constexpr auto matrix() const noexcept -> ColourMatrix { return matrix_; }

  constexpr auto range() const noexcept -> ColourRange { return range_; }

  constexpr auto red_weight() const noexcept -> float {
    return luma_weights()[0];
  }

  constexpr auto blue_weight() const noexcept -> float {
    return luma_weights()[1];
  }

  constexpr auto levels() const noexcept -> Packed {
    if (range_ == ColourRange::full) {
      return {1.0F, 0.0F, 1.0F, chroma_zero};
    }
    return {code_span / limited_luma_span, limited_black,
            code_span / limited_chroma_span, chroma_zero};
  }

  constexpr auto weights() const noexcept -> Packed {
    const float red = red_weight();
    const float blue = blue_weight();
    const float green = 1.0F - red - blue;
    return {2.0F * (1.0F - red), 2.0F * blue * (1.0F - blue) / green,
            2.0F * red * (1.0F - red) / green, 2.0F * (1.0F - blue)};
  }

  static constexpr auto rgb(const Packed& levels, const Packed& weights,
                            float luma, float blue_difference,
                            float red_difference) noexcept -> Rgb {
    const float y = (luma - levels[1]) * levels[0];
    const float cb = (blue_difference - levels[3]) * levels[2];
    const float cr = (red_difference - levels[3]) * levels[2];
    return {std::clamp(y + (weights[0] * cr), 0.0F, 1.0F),
            std::clamp(y - (weights[1] * cb) - (weights[2] * cr), 0.0F, 1.0F),
            std::clamp(y + (weights[3] * cb), 0.0F, 1.0F)};
  }

  constexpr auto rgb(float luma, float blue_difference,
                     float red_difference) const noexcept -> Rgb {
    return rgb(levels(), weights(), luma, blue_difference, red_difference);
  }

  friend constexpr auto operator==(const YuvConversion&, const YuvConversion&)
      -> bool = default;

 private:
  constexpr auto luma_weights() const noexcept -> std::array<float, 2> {
    switch (matrix_) {
      case ColourMatrix::bt601:
        return {0.299F, 0.114F};
      case ColourMatrix::bt709:
        return {0.2126F, 0.0722F};
      case ColourMatrix::bt2020:
        return {0.2627F, 0.0593F};
    }
    return {0.2126F, 0.0722F};
  }

  ColourMatrix matrix_{ColourMatrix::bt709};
  ColourRange range_{ColourRange::limited};
};

}
