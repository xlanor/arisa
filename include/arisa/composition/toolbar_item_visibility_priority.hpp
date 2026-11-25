#pragma once

#include <compare>
#include <cstdint>

namespace arisa::composition {

class ToolbarItemVisibilityPriority {
 public:
  static const ToolbarItemVisibilityPriority low;
  static const ToolbarItemVisibilityPriority automatic;
  static const ToolbarItemVisibilityPriority high;

  constexpr ToolbarItemVisibilityPriority() noexcept = default;

  static constexpr auto higher_than(
      ToolbarItemVisibilityPriority other) noexcept
      -> ToolbarItemVisibilityPriority {
    return ToolbarItemVisibilityPriority{
        static_cast<std::int16_t>(other.rank_ + 1)};
  }

  static constexpr auto lower_than(ToolbarItemVisibilityPriority other) noexcept
      -> ToolbarItemVisibilityPriority {
    return ToolbarItemVisibilityPriority{
        static_cast<std::int16_t>(other.rank_ - 1)};
  }

  constexpr auto rank() const noexcept -> std::int16_t { return rank_; }

  friend constexpr auto operator==(ToolbarItemVisibilityPriority,
                                   ToolbarItemVisibilityPriority)
      -> bool = default;

  friend constexpr auto operator<=>(ToolbarItemVisibilityPriority,
                                    ToolbarItemVisibilityPriority) = default;

 private:
  static constexpr std::int16_t step = 100;

  constexpr explicit ToolbarItemVisibilityPriority(std::int16_t rank) noexcept
      : rank_(rank) {}

  std::int16_t rank_{0};
};

inline constexpr ToolbarItemVisibilityPriority
    ToolbarItemVisibilityPriority::low{static_cast<std::int16_t>(-step)};
inline constexpr ToolbarItemVisibilityPriority
    ToolbarItemVisibilityPriority::automatic{0};
inline constexpr ToolbarItemVisibilityPriority
    ToolbarItemVisibilityPriority::high{step};

}
