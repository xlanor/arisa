#pragma once

#include <chrono>

namespace arisa::provider {

struct NavigationTuning {
  std::chrono::milliseconds repeat_delay{400};
  std::chrono::milliseconds repeat_interval{120};
  std::chrono::milliseconds repeat_minimum{40};
  float repeat_acceleration{0.8F};
  std::chrono::milliseconds long_press{500};
  float stick_engage{0.5F};
  float stick_release{0.3F};
  float direction_hysteresis{1.35F};
  float scroll_speed{1600.0F};

  friend constexpr auto operator==(const NavigationTuning&,
                                   const NavigationTuning&) -> bool = default;
};

}
