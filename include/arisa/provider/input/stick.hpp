#pragma once

namespace arisa::provider {

struct Stick {
  float x{0.0F};
  float y{0.0F};

  friend constexpr auto operator==(const Stick&, const Stick&)
      -> bool = default;
};

}
