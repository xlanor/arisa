#pragma once

#include <cstdint>

namespace arisa::composition {

class RedactionReasons {
 public:
  static const RedactionReasons placeholder;
  static const RedactionReasons privacy;
  static const RedactionReasons invalidated;

  constexpr RedactionReasons() noexcept = default;

  constexpr auto empty() const noexcept -> bool { return bits_ == 0; }

  constexpr auto contains(RedactionReasons other) const noexcept -> bool {
    return (bits_ & other.bits_) == other.bits_ && !other.empty();
  }

  constexpr auto redacts(bool sensitive) const noexcept -> bool {
    return contains(placeholder) || (sensitive && contains(privacy));
  }

  friend constexpr auto operator|(RedactionReasons left,
                                  RedactionReasons right) noexcept
      -> RedactionReasons {
    return RedactionReasons{
        static_cast<std::uint8_t>(left.bits_ | right.bits_)};
  }

  friend constexpr auto operator==(RedactionReasons, RedactionReasons)
      -> bool = default;

 private:
  constexpr explicit RedactionReasons(std::uint8_t bits) noexcept
      : bits_(bits) {}

  std::uint8_t bits_{0};
};

inline constexpr RedactionReasons RedactionReasons::placeholder{1U};
inline constexpr RedactionReasons RedactionReasons::privacy{2U};
inline constexpr RedactionReasons RedactionReasons::invalidated{4U};

}
