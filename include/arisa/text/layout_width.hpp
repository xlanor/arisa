#pragma once

#include <limits>
#include <optional>

namespace arisa::text {

class LayoutWidth {
 public:
  static constexpr auto unbounded() noexcept -> LayoutWidth {
    return LayoutWidth{std::numeric_limits<float>::infinity()};
  }

  static auto points(float value) noexcept -> std::optional<LayoutWidth>;

  constexpr auto bounded() const noexcept -> bool {
    return points_ != std::numeric_limits<float>::infinity();
  }

  constexpr auto value() const noexcept -> float { return points_; }

  friend constexpr auto operator==(LayoutWidth, LayoutWidth) -> bool = default;

 private:
  constexpr explicit LayoutWidth(float points) noexcept : points_(points) {}

  float points_;
};

}
