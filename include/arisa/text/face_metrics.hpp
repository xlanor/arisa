#pragma once

namespace arisa::text {

struct FaceMetrics {
  float ascent{0.0F};
  float descent{0.0F};
  float line_gap{0.0F};
  float underline_position{0.0F};
  float underline_thickness{0.0F};
  float strikeout_position{0.0F};
  float strikeout_thickness{0.0F};

  friend constexpr auto operator==(const FaceMetrics&, const FaceMetrics&)
      -> bool = default;
};

}
