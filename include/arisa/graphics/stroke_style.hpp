#pragma once

#include <cstdint>
#include <vector>

namespace arisa::graphics {

enum class LineCap : std::uint8_t {
  butt,
  round,
  square,
};

enum class LineJoin : std::uint8_t {
  miter,
  round,
  bevel,
};

struct StrokeStyle {
  float line_width{1.0F};
  LineCap line_cap{LineCap::butt};
  LineJoin line_join{LineJoin::miter};
  float miter_limit{10.0F};
  std::vector<float> dash{};
  float dash_phase{0.0F};

  friend auto operator==(const StrokeStyle&, const StrokeStyle&)
      -> bool = default;
};

struct FillStyle {
  bool eo_fill{false};
  bool antialiased{true};

  friend constexpr auto operator==(const FillStyle&, const FillStyle&)
      -> bool = default;
};

}
