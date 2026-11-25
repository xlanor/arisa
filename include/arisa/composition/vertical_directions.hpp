#pragma once

#include <arisa/composition/vertical_direction.hpp>

#include <cstdint>

namespace arisa::composition {

class VerticalDirections {
 public:
  static const VerticalDirections up;
  static const VerticalDirections down;
  static const VerticalDirections all;

  constexpr VerticalDirections() noexcept = default;

  constexpr VerticalDirections(VerticalDirection direction) noexcept
      : bits_(direction == VerticalDirection::up ? up_bit : down_bit) {}

  constexpr auto contains(VerticalDirections other) const noexcept -> bool {
    return other.bits_ != 0U && (bits_ & other.bits_) == other.bits_;
  }

  constexpr auto empty() const noexcept -> bool { return bits_ == 0U; }

  friend constexpr auto operator|(VerticalDirections left,
                                  VerticalDirections right) noexcept
      -> VerticalDirections {
    return VerticalDirections{
        static_cast<std::uint8_t>(left.bits_ | right.bits_)};
  }

  friend constexpr auto operator==(VerticalDirections, VerticalDirections)
      -> bool = default;

 private:
  static constexpr std::uint8_t up_bit = 1U;
  static constexpr std::uint8_t down_bit = 2U;

  constexpr explicit VerticalDirections(std::uint8_t bits) noexcept
      : bits_(bits) {}

  std::uint8_t bits_{0};
};

inline constexpr VerticalDirections VerticalDirections::up{
    VerticalDirection::up};
inline constexpr VerticalDirections VerticalDirections::down{
    VerticalDirection::down};
inline constexpr VerticalDirections VerticalDirections::all{
    VerticalDirections::up | VerticalDirections::down};

}
