#pragma once

#include <arisa/composition/point.hpp>

#include <optional>

namespace arisa::composition {

class HoverPhase {
 public:
  static const HoverPhase ended;

  static constexpr auto active(Point location) noexcept -> HoverPhase {
    return HoverPhase{location};
  }

  constexpr auto is_active() const noexcept -> bool {
    return location_.has_value();
  }

  constexpr auto location() const noexcept -> std::optional<Point> {
    return location_;
  }

  friend constexpr auto operator==(const HoverPhase&, const HoverPhase&)
      -> bool = default;

 private:
  constexpr HoverPhase() noexcept = default;

  constexpr explicit HoverPhase(Point location) noexcept
      : location_(location) {}

  std::optional<Point> location_;
};

inline constexpr HoverPhase HoverPhase::ended{};

}
