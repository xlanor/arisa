#pragma once

#include <algorithm>

namespace arisa::composition {

class ScrollTransitionPhase {
 public:
  static const ScrollTransitionPhase top_leading;
  static const ScrollTransitionPhase identity;
  static const ScrollTransitionPhase bottom_trailing;

  static constexpr auto at(double value) noexcept -> ScrollTransitionPhase {
    return ScrollTransitionPhase{std::clamp(value, -1.0, 1.0)};
  }

  constexpr auto value() const noexcept -> double { return value_; }

  constexpr auto is_identity() const noexcept -> bool { return value_ == 0.0; }

  constexpr auto is_top_leading() const noexcept -> bool {
    return value_ < 0.0;
  }

  constexpr auto is_bottom_trailing() const noexcept -> bool {
    return value_ > 0.0;
  }

  friend constexpr auto operator==(ScrollTransitionPhase, ScrollTransitionPhase)
      -> bool = default;

 private:
  constexpr explicit ScrollTransitionPhase(double value) noexcept
      : value_(value) {}

  double value_{0.0};
};

inline constexpr ScrollTransitionPhase ScrollTransitionPhase::top_leading{-1.0};
inline constexpr ScrollTransitionPhase ScrollTransitionPhase::identity{0.0};
inline constexpr ScrollTransitionPhase ScrollTransitionPhase::bottom_trailing{
    1.0};

}
