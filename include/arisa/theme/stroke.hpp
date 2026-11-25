#pragma once

#include <optional>

namespace arisa::theme {

class Stroke {
 public:
  constexpr Stroke(float points) noexcept : points_(points) {}

  constexpr Stroke(int points) noexcept : points_(static_cast<float>(points)) {}

  constexpr Stroke(double points) noexcept
      : points_(static_cast<float>(points)) {}

  static constexpr auto hairline() noexcept -> Stroke { return Stroke{}; }

  constexpr auto is_hairline() const noexcept -> bool {
    return !points_.has_value();
  }

  constexpr auto device_pixels(float scale) const noexcept -> float {
    return points_.has_value() ? *points_ * scale : 1.0F;
  }

  friend constexpr auto operator==(const Stroke&, const Stroke&)
      -> bool = default;

 private:
  constexpr Stroke() noexcept = default;

  std::optional<float> points_;
};

}
