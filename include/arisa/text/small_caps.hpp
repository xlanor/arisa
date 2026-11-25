#pragma once

#include <cstdint>

namespace arisa::text {

enum class SmallCaps : std::uint8_t {
  none = 0,
  lowercase = 1,
  uppercase = 2,
  all = 3,
};

class SmallCapitals {
 public:
  static constexpr float scale = 0.8F;

  static constexpr auto lowers(SmallCaps caps) noexcept -> bool {
    return (static_cast<std::uint8_t>(caps) & 1U) != 0;
  }

  static constexpr auto uppers(SmallCaps caps) noexcept -> bool {
    return (static_cast<std::uint8_t>(caps) & 2U) != 0;
  }

  static constexpr auto combined(SmallCaps first, SmallCaps second) noexcept
      -> SmallCaps {
    return static_cast<SmallCaps>(static_cast<std::uint8_t>(first) |
                                  static_cast<std::uint8_t>(second));
  }
};

}
