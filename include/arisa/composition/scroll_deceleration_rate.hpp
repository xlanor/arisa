#pragma once

#include <algorithm>

namespace arisa::composition {

class ScrollDecelerationRate {
 public:
  static constexpr double normal_value = 0.998;
  static constexpr double fast_value = 0.99;

  constexpr explicit ScrollDecelerationRate(double per_millisecond) noexcept
      : value_(std::clamp(per_millisecond, minimum, maximum)) {}

  static constexpr auto normal() noexcept -> ScrollDecelerationRate {
    return ScrollDecelerationRate{normal_value};
  }

  static constexpr auto fast() noexcept -> ScrollDecelerationRate {
    return ScrollDecelerationRate{fast_value};
  }

  constexpr auto value() const noexcept -> double { return value_; }

  friend constexpr auto operator==(ScrollDecelerationRate,
                                   ScrollDecelerationRate) -> bool = default;

 private:
  static constexpr double minimum = 0.5;
  static constexpr double maximum = 0.9999;

  double value_;
};

}
