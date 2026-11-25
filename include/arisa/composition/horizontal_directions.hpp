#pragma once

#include <arisa/composition/horizontal_direction.hpp>

#include <cstdint>

namespace arisa::composition {

class HorizontalDirections {
 public:
  static const HorizontalDirections leading;
  static const HorizontalDirections trailing;
  static const HorizontalDirections all;

  constexpr HorizontalDirections() noexcept = default;

  constexpr HorizontalDirections(HorizontalDirection direction) noexcept
      : bits_(direction == HorizontalDirection::leading ? leading_bit
                                                        : trailing_bit) {}

  constexpr auto contains(HorizontalDirections other) const noexcept -> bool {
    return other.bits_ != 0U && (bits_ & other.bits_) == other.bits_;
  }

  constexpr auto empty() const noexcept -> bool { return bits_ == 0U; }

  friend constexpr auto operator|(HorizontalDirections left,
                                  HorizontalDirections right) noexcept
      -> HorizontalDirections {
    return HorizontalDirections{
        static_cast<std::uint8_t>(left.bits_ | right.bits_)};
  }

  friend constexpr auto operator==(HorizontalDirections, HorizontalDirections)
      -> bool = default;

 private:
  static constexpr std::uint8_t leading_bit = 1U;
  static constexpr std::uint8_t trailing_bit = 2U;

  constexpr explicit HorizontalDirections(std::uint8_t bits) noexcept
      : bits_(bits) {}

  std::uint8_t bits_{0};
};

inline constexpr HorizontalDirections HorizontalDirections::leading{
    HorizontalDirection::leading};
inline constexpr HorizontalDirections HorizontalDirections::trailing{
    HorizontalDirection::trailing};
inline constexpr HorizontalDirections HorizontalDirections::all{
    HorizontalDirections::leading | HorizontalDirections::trailing};

}
