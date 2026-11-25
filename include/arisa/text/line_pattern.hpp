#pragma once

#include <array>
#include <cstdint>
#include <span>

namespace arisa::text {

enum class LinePattern : std::uint8_t {
  solid,
  dot,
  dash,
  dash_dot,
  dash_dot_dot,
};

class LineDashes {
 public:
  static constexpr float dot = 0.3F;
  static constexpr float dash = 1.0F;
  static constexpr float dash_gap = 0.5F;
  static constexpr float gap = 0.3F;

  static constexpr auto of(LinePattern pattern) noexcept
      -> std::span<const float> {
    switch (pattern) {
      case LinePattern::solid:
        return {};
      case LinePattern::dot:
        return dots;
      case LinePattern::dash:
        return dashes;
      case LinePattern::dash_dot:
        return dash_dots;
      case LinePattern::dash_dot_dot:
        return dash_dot_dots;
    }
    return {};
  }

 private:
  static constexpr std::array<float, 2> dots{dot, dot};
  static constexpr std::array<float, 2> dashes{dash, dash_gap};
  static constexpr std::array<float, 4> dash_dots{dash, gap, dot, gap};
  static constexpr std::array<float, 6> dash_dot_dots{dash, gap, dot,
                                                      gap,  dot, gap};
};

}
