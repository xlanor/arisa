#pragma once

#include <compare>

namespace arisa::provider {

class HapticStrength {
 public:
  static constexpr int minimum_percent = 0;
  static constexpr int maximum_percent = 200;
  static constexpr int step_percent = 5;
  static constexpr float minimum = 0.0F;
  static constexpr float maximum = 2.0F;
  static constexpr float step = 0.05F;

  constexpr HapticStrength() noexcept = default;

  constexpr explicit HapticStrength(float value) noexcept
      : percent_(snapped(value)) {}

  static constexpr auto from_percent(int percent) noexcept -> HapticStrength {
    return HapticStrength{static_cast<float>(percent) / 100.0F};
  }

  constexpr auto value() const noexcept -> float {
    return static_cast<float>(percent_) / 100.0F;
  }

  constexpr auto percent() const noexcept -> int { return percent_; }

  constexpr auto silent() const noexcept -> bool { return percent_ == 0; }

  friend constexpr auto operator==(HapticStrength, HapticStrength)
      -> bool = default;
  friend constexpr auto operator<=>(HapticStrength, HapticStrength) = default;

 private:
  static constexpr auto snapped(float value) noexcept -> int {
    if (!(value > minimum)) {
      return minimum_percent;
    }
    if (value >= maximum) {
      return maximum_percent;
    }
    const auto steps = static_cast<int>((value / step) + 0.5F);
    return steps * step_percent;
  }

  int percent_{100};
};

}
