#pragma once

#include <array>
#include <cstdint>

namespace arisa::composition {

enum class HorizontalAlignment : std::uint8_t {
  leading,
  center,
  trailing,
};

enum class VerticalAlignment : std::uint8_t {
  top,
  center,
  bottom,
  first_text_baseline,
  last_text_baseline,
};

enum class Distribution : std::uint8_t {
  start,
  center,
  end,
  space_between,
  space_around,
  space_evenly,
};

enum class LayoutDirection : std::uint8_t {
  left_to_right,
  right_to_left,
};

inline constexpr std::array<LayoutDirection, 2> all_layout_directions{
    LayoutDirection::left_to_right, LayoutDirection::right_to_left};

struct Alignment {
  HorizontalAlignment horizontal{HorizontalAlignment::center};
  VerticalAlignment vertical{VerticalAlignment::center};

  static const Alignment top_leading;
  static const Alignment top;
  static const Alignment top_trailing;
  static const Alignment leading;
  static const Alignment center;
  static const Alignment trailing;
  static const Alignment bottom_leading;
  static const Alignment bottom;
  static const Alignment bottom_trailing;

  friend constexpr auto operator==(const Alignment&, const Alignment&)
      -> bool = default;
};

inline constexpr Alignment Alignment::top_leading{HorizontalAlignment::leading,
                                                  VerticalAlignment::top};
inline constexpr Alignment Alignment::top{HorizontalAlignment::center,
                                          VerticalAlignment::top};
inline constexpr Alignment Alignment::top_trailing{
    HorizontalAlignment::trailing, VerticalAlignment::top};
inline constexpr Alignment Alignment::leading{HorizontalAlignment::leading,
                                              VerticalAlignment::center};
inline constexpr Alignment Alignment::center{HorizontalAlignment::center,
                                             VerticalAlignment::center};
inline constexpr Alignment Alignment::trailing{HorizontalAlignment::trailing,
                                               VerticalAlignment::center};
inline constexpr Alignment Alignment::bottom_leading{
    HorizontalAlignment::leading, VerticalAlignment::bottom};
inline constexpr Alignment Alignment::bottom{HorizontalAlignment::center,
                                             VerticalAlignment::bottom};
inline constexpr Alignment Alignment::bottom_trailing{
    HorizontalAlignment::trailing, VerticalAlignment::bottom};

}
