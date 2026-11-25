#pragma once

namespace arisa::runtime {

struct PassedInput {
  bool left_stick{false};
  bool right_stick{false};
  bool touch{false};
  bool pointer{false};

  friend constexpr auto operator==(PassedInput, PassedInput) -> bool = default;
};

}
