#pragma once

namespace arisa::motion {

struct UnitPoint {
  float x{0.5F};
  float y{0.5F};

  static const UnitPoint zero;
  static const UnitPoint center;
  static const UnitPoint leading;
  static const UnitPoint trailing;
  static const UnitPoint top;
  static const UnitPoint bottom;
  static const UnitPoint top_leading;
  static const UnitPoint top_trailing;
  static const UnitPoint bottom_leading;
  static const UnitPoint bottom_trailing;

  friend constexpr auto operator==(const UnitPoint&, const UnitPoint&)
      -> bool = default;
};

inline constexpr UnitPoint UnitPoint::zero{0.0F, 0.0F};
inline constexpr UnitPoint UnitPoint::center{0.5F, 0.5F};
inline constexpr UnitPoint UnitPoint::leading{0.0F, 0.5F};
inline constexpr UnitPoint UnitPoint::trailing{1.0F, 0.5F};
inline constexpr UnitPoint UnitPoint::top{0.5F, 0.0F};
inline constexpr UnitPoint UnitPoint::bottom{0.5F, 1.0F};
inline constexpr UnitPoint UnitPoint::top_leading{0.0F, 0.0F};
inline constexpr UnitPoint UnitPoint::top_trailing{1.0F, 0.0F};
inline constexpr UnitPoint UnitPoint::bottom_leading{0.0F, 1.0F};
inline constexpr UnitPoint UnitPoint::bottom_trailing{1.0F, 1.0F};

}
