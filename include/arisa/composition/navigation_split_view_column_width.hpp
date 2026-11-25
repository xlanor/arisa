#pragma once

#include <algorithm>
#include <optional>

namespace arisa::composition {

class NavigationSplitViewColumnWidth {
 public:
  constexpr explicit NavigationSplitViewColumnWidth(float width) noexcept
      : ideal_(width) {}

  constexpr NavigationSplitViewColumnWidth(
      std::optional<float> minimum, float ideal,
      std::optional<float> maximum) noexcept
      : minimum_(minimum), ideal_(ideal), maximum_(maximum) {}

  constexpr auto resolved() const noexcept -> float {
    float width = ideal_;
    if (maximum_.has_value()) {
      width = std::min(width, *maximum_);
    }
    if (minimum_.has_value()) {
      width = std::max(width, *minimum_);
    }
    return width;
  }

  constexpr auto resizable() const noexcept -> bool {
    return minimum_.has_value() || maximum_.has_value();
  }

  constexpr auto lower() const noexcept -> float {
    return std::min(minimum_.value_or(resolved()), resolved());
  }

  constexpr auto upper() const noexcept -> float {
    return std::max(maximum_.value_or(resolved()), resolved());
  }

  constexpr auto clamped(float width) const noexcept -> float {
    return std::clamp(width, lower(), upper());
  }

  friend constexpr auto operator==(const NavigationSplitViewColumnWidth&,
                                   const NavigationSplitViewColumnWidth&)
      -> bool = default;

 private:
  std::optional<float> minimum_;
  float ideal_;
  std::optional<float> maximum_;
};

}
