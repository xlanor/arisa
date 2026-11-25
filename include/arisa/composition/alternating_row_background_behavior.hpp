#pragma once

#include <cstdint>

namespace arisa::composition {

class AlternatingRowBackgroundBehavior {
 public:
  static const AlternatingRowBackgroundBehavior automatic;
  static const AlternatingRowBackgroundBehavior enabled;
  static const AlternatingRowBackgroundBehavior disabled;

  constexpr auto alternates(bool by_default) const noexcept -> bool {
    return level_ == Level::enabled ||
           (level_ == Level::automatic && by_default);
  }

  friend constexpr auto operator==(const AlternatingRowBackgroundBehavior&,
                                   const AlternatingRowBackgroundBehavior&)
      -> bool = default;

 private:
  enum class Level : std::uint8_t { automatic, enabled, disabled };

  constexpr explicit AlternatingRowBackgroundBehavior(Level level) noexcept
      : level_(level) {}

  Level level_;
};

inline constexpr AlternatingRowBackgroundBehavior
    AlternatingRowBackgroundBehavior::automatic{
        AlternatingRowBackgroundBehavior::Level::automatic};
inline constexpr AlternatingRowBackgroundBehavior
    AlternatingRowBackgroundBehavior::enabled{
        AlternatingRowBackgroundBehavior::Level::enabled};
inline constexpr AlternatingRowBackgroundBehavior
    AlternatingRowBackgroundBehavior::disabled{
        AlternatingRowBackgroundBehavior::Level::disabled};

}
