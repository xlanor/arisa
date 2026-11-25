#pragma once

#include <cstdint>

namespace arisa::composition {

class SafeAreaRegions {
 public:
  static const SafeAreaRegions container;
  static const SafeAreaRegions keyboard;
  static const SafeAreaRegions all;

  constexpr SafeAreaRegions() noexcept = default;

  constexpr explicit SafeAreaRegions(std::uint8_t raw_value) noexcept
      : bits_(raw_value) {}

  constexpr auto contains(SafeAreaRegions other) const noexcept -> bool {
    return (bits_ & other.bits_) == other.bits_ && !other.empty();
  }

  constexpr auto empty() const noexcept -> bool { return bits_ == 0U; }

  constexpr auto raw_value() const noexcept -> std::uint8_t { return bits_; }

  friend constexpr auto operator|(SafeAreaRegions left,
                                  SafeAreaRegions right) noexcept
      -> SafeAreaRegions {
    return SafeAreaRegions{static_cast<std::uint8_t>(left.bits_ | right.bits_)};
  }

  friend constexpr auto operator&(SafeAreaRegions left,
                                  SafeAreaRegions right) noexcept
      -> SafeAreaRegions {
    return SafeAreaRegions{static_cast<std::uint8_t>(left.bits_ & right.bits_)};
  }

  friend constexpr auto operator==(SafeAreaRegions, SafeAreaRegions)
      -> bool = default;

 private:
  std::uint8_t bits_{0};
};

inline constexpr SafeAreaRegions SafeAreaRegions::container{1U};
inline constexpr SafeAreaRegions SafeAreaRegions::keyboard{2U};
inline constexpr SafeAreaRegions SafeAreaRegions::all{0xFFU};

}
