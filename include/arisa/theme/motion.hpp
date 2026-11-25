#pragma once

namespace arisa::theme {

struct SpringToken {
  double response{0.5};
  double damping_ratio{1.0};

  friend constexpr auto operator==(const SpringToken&, const SpringToken&)
      -> bool = default;
};

struct Motion {
  SpringToken focus{.response = 0.35, .damping_ratio = 0.72};
  SpringToken standard{.response = 0.5, .damping_ratio = 1.0};

  friend constexpr auto operator==(const Motion&, const Motion&)
      -> bool = default;
};

}
