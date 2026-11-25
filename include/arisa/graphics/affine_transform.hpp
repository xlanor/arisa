#pragma once

#include <arisa/composition/point.hpp>
#include <arisa/motion/angle.hpp>

#include <cmath>

namespace arisa::graphics {

class AffineTransform {
 public:
  constexpr AffineTransform() noexcept = default;

  constexpr AffineTransform(float a, float b, float c, float d, float tx,
                            float ty) noexcept
      : a_(a), b_(b), c_(c), d_(d), tx_(tx), ty_(ty) {}

  static constexpr auto identity() noexcept -> AffineTransform {
    return AffineTransform{};
  }

  static constexpr auto translation(float x, float y) noexcept
      -> AffineTransform {
    return AffineTransform{1.0F, 0.0F, 0.0F, 1.0F, x, y};
  }

  static constexpr auto scale(float x, float y) noexcept -> AffineTransform {
    return AffineTransform{x, 0.0F, 0.0F, y, 0.0F, 0.0F};
  }

  static auto rotation(motion::Angle angle) noexcept -> AffineTransform {
    const auto c = static_cast<float>(std::cos(angle.radians()));
    const auto s = static_cast<float>(std::sin(angle.radians()));
    return AffineTransform{c, s, -s, c, 0.0F, 0.0F};
  }

  constexpr auto concatenating(const AffineTransform& after) const noexcept
      -> AffineTransform {
    return AffineTransform{(a_ * after.a_) + (b_ * after.c_),
                           (a_ * after.b_) + (b_ * after.d_),
                           (c_ * after.a_) + (d_ * after.c_),
                           (c_ * after.b_) + (d_ * after.d_),
                           (tx_ * after.a_) + (ty_ * after.c_) + after.tx_,
                           (tx_ * after.b_) + (ty_ * after.d_) + after.ty_};
  }

  constexpr auto translated_by(float x, float y) const noexcept
      -> AffineTransform {
    return translation(x, y).concatenating(*this);
  }

  constexpr auto scaled_by(float x, float y) const noexcept -> AffineTransform {
    return scale(x, y).concatenating(*this);
  }

  auto rotated_by(motion::Angle angle) const noexcept -> AffineTransform {
    return rotation(angle).concatenating(*this);
  }

  constexpr auto apply(composition::Point point) const noexcept
      -> composition::Point {
    return composition::Point{.x = (a_ * point.x) + (c_ * point.y) + tx_,
                              .y = (b_ * point.x) + (d_ * point.y) + ty_};
  }

  constexpr auto is_identity() const noexcept -> bool {
    return *this == AffineTransform{};
  }

  constexpr auto a() const noexcept -> float { return a_; }
  constexpr auto b() const noexcept -> float { return b_; }
  constexpr auto c() const noexcept -> float { return c_; }
  constexpr auto d() const noexcept -> float { return d_; }
  constexpr auto tx() const noexcept -> float { return tx_; }
  constexpr auto ty() const noexcept -> float { return ty_; }

  friend constexpr auto operator==(const AffineTransform&,
                                   const AffineTransform&) -> bool = default;

 private:
  float a_{1.0F};
  float b_{0.0F};
  float c_{0.0F};
  float d_{1.0F};
  float tx_{0.0F};
  float ty_{0.0F};
};

}
