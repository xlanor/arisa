#pragma once

#include <algorithm>

namespace arisa::controls {

template <class T>
struct ClosedRange {
  T lower_bound{};
  T upper_bound{};

  constexpr auto clamped(T value) const noexcept -> T {
    return std::clamp(value, lower_bound, upper_bound);
  }

  friend constexpr auto operator==(const ClosedRange&, const ClosedRange&)
      -> bool = default;
};

}
