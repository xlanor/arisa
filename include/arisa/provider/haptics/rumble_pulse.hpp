#pragma once

#include <chrono>

namespace arisa::provider {

struct RumblePulse {
  float low_amplitude{0.0F};
  float low_frequency{160.0F};
  float high_amplitude{0.0F};
  float high_frequency{320.0F};
  std::chrono::milliseconds duration{0};

  friend constexpr auto operator==(const RumblePulse&, const RumblePulse&)
      -> bool = default;
};

}
