#pragma once

#include <compare>
#include <numbers>

namespace arisa::motion {

class Angle {
 public:
  constexpr Angle() noexcept = default;

  static constexpr auto radians(double value) noexcept -> Angle {
    return Angle{value};
  }

  static constexpr auto degrees(double value) noexcept -> Angle {
    return Angle{value * std::numbers::pi / 180.0};
  }

  constexpr auto radians() const noexcept -> double { return radians_; }

  constexpr auto degrees() const noexcept -> double {
    return radians_ * 180.0 / std::numbers::pi;
  }

  friend constexpr auto operator+(Angle left, Angle right) noexcept -> Angle {
    return Angle{left.radians_ + right.radians_};
  }

  friend constexpr auto operator==(Angle, Angle) -> bool = default;

  friend constexpr auto operator<=>(Angle left, Angle right) noexcept
      -> std::partial_ordering {
    return left.radians_ <=> right.radians_;
  }

 private:
  constexpr explicit Angle(double value) noexcept : radians_(value) {}

  double radians_{0.0};
};

}
