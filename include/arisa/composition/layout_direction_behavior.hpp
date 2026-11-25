#pragma once

#include <arisa/composition/alignment.hpp>

#include <optional>

namespace arisa::composition {

class LayoutDirectionBehavior {
 public:
  static constexpr auto fixed() noexcept -> LayoutDirectionBehavior {
    return LayoutDirectionBehavior{std::nullopt};
  }

  static constexpr auto mirrors(
      LayoutDirection direction = LayoutDirection::right_to_left) noexcept
      -> LayoutDirectionBehavior {
    return LayoutDirectionBehavior{direction};
  }

  constexpr auto mirrored_in(LayoutDirection direction) const noexcept -> bool {
    return mirrors_.has_value() && *mirrors_ == direction;
  }

  constexpr auto is_fixed() const noexcept -> bool {
    return !mirrors_.has_value();
  }

  friend constexpr auto operator==(LayoutDirectionBehavior,
                                   LayoutDirectionBehavior) -> bool = default;

 private:
  constexpr explicit LayoutDirectionBehavior(
      std::optional<LayoutDirection> mirrors) noexcept
      : mirrors_(mirrors) {}

  std::optional<LayoutDirection> mirrors_;
};

}
