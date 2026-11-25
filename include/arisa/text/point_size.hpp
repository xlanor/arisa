#pragma once

#include <compare>
#include <optional>

namespace arisa::text {

class PointSize {
 public:
  static constexpr float maximum = 512.0F;

  static auto create(float points) noexcept -> std::optional<PointSize>;

  template <float Points>
    requires(Points > 0.0F && Points <= maximum)
  static consteval auto of() noexcept -> PointSize {
    return PointSize{Points};
  }

  constexpr auto value() const noexcept -> float { return points_; }

  friend constexpr auto operator==(PointSize, PointSize) -> bool = default;
  friend constexpr auto operator<=>(PointSize, PointSize) = default;

 private:
  constexpr explicit PointSize(float points) noexcept : points_(points) {}

  float points_;
};

}
