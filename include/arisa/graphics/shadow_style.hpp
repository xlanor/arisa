#pragma once

#include <arisa/theme/colour_value.hpp>

#include <algorithm>
#include <cstdint>

namespace arisa::graphics {

class ShadowStyle {
 public:
  enum class Kind : std::uint8_t {
    drop,
    inner,
  };

  static constexpr auto drop(
      theme::ColourValue colour = theme::ColourRole::shadow,
      float radius = 0.0F, float x = 0.0F, float y = 0.0F) noexcept
      -> ShadowStyle {
    return ShadowStyle{Kind::drop, colour, radius, x, y};
  }

  static constexpr auto inner(
      theme::ColourValue colour = theme::ColourRole::shadow,
      float radius = 0.0F, float x = 0.0F, float y = 0.0F) noexcept
      -> ShadowStyle {
    return ShadowStyle{Kind::inner, colour, radius, x, y};
  }

  constexpr auto kind() const noexcept -> Kind { return kind_; }

  constexpr auto colour() const noexcept -> theme::ColourValue {
    return colour_;
  }

  constexpr auto radius() const noexcept -> float { return radius_; }

  constexpr auto x() const noexcept -> float { return x_; }

  constexpr auto y() const noexcept -> float { return y_; }

  friend constexpr auto operator==(const ShadowStyle&, const ShadowStyle&)
      -> bool = default;

 private:
  constexpr ShadowStyle(Kind kind, theme::ColourValue colour, float radius,
                        float x, float y) noexcept
      : kind_(kind),
        colour_(colour),
        radius_(std::max(radius, 0.0F)),
        x_(x),
        y_(y) {}

  Kind kind_;
  theme::ColourValue colour_;
  float radius_;
  float x_;
  float y_;
};

}
