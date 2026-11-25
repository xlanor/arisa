#pragma once

#include <arisa/composition/axis.hpp>

#include <cstdint>

namespace arisa::composition {

class Axes {
 public:
  static const Axes horizontal;
  static const Axes vertical;
  static const Axes all;

  constexpr Axes() noexcept = default;

  constexpr Axes(Axis axis) noexcept
      : bits_(axis == Axis::horizontal ? horizontal_bit : vertical_bit) {}

  constexpr auto contains(Axis axis) const noexcept -> bool {
    return (bits_ & Axes{axis}.bits_) != 0U;
  }

  constexpr auto empty() const noexcept -> bool { return bits_ == 0U; }

  friend constexpr auto operator|(Axes left, Axes right) noexcept -> Axes {
    return Axes{static_cast<std::uint8_t>(left.bits_ | right.bits_)};
  }

  friend constexpr auto operator==(Axes, Axes) -> bool = default;

 private:
  static constexpr std::uint8_t horizontal_bit = 1U;
  static constexpr std::uint8_t vertical_bit = 2U;

  constexpr explicit Axes(std::uint8_t bits) noexcept : bits_(bits) {}

  std::uint8_t bits_{0};
};

inline constexpr Axes Axes::horizontal{Axis::horizontal};
inline constexpr Axes Axes::vertical{Axis::vertical};
inline constexpr Axes Axes::all{Axes::horizontal | Axes::vertical};

}
