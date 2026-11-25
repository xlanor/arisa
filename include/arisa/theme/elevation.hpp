#pragma once

#include <cstddef>
#include <cstdint>

namespace arisa::theme {

enum class Elevation : std::uint8_t {
  none,
  low,
  medium,
  high,
};

inline constexpr std::size_t elevation_count = 4;

struct Shadow {
  float offset_y{0.0F};
  float blur{0.0F};
  float opacity{0.0F};

  friend constexpr auto operator==(const Shadow&, const Shadow&)
      -> bool = default;
};

struct ElevationStyle {
  Shadow ambient;
  Shadow key;

  friend constexpr auto operator==(const ElevationStyle&, const ElevationStyle&)
      -> bool = default;
};

}
