#pragma once

#include <arisa/composition/frame_resize_direction.hpp>

#include <cstdint>

namespace arisa::composition {

class FrameResizeDirections {
 public:
  static const FrameResizeDirections inward;
  static const FrameResizeDirections outward;
  static const FrameResizeDirections all;

  constexpr FrameResizeDirections() noexcept = default;

  constexpr FrameResizeDirections(FrameResizeDirection direction) noexcept
      : bits_(direction == FrameResizeDirection::inward ? inward_bit
                                                        : outward_bit) {}

  constexpr auto contains(FrameResizeDirections other) const noexcept -> bool {
    return other.bits_ != 0U && (bits_ & other.bits_) == other.bits_;
  }

  constexpr auto empty() const noexcept -> bool { return bits_ == 0U; }

  friend constexpr auto operator|(FrameResizeDirections left,
                                  FrameResizeDirections right) noexcept
      -> FrameResizeDirections {
    return FrameResizeDirections{
        static_cast<std::uint8_t>(left.bits_ | right.bits_)};
  }

  friend constexpr auto operator==(FrameResizeDirections, FrameResizeDirections)
      -> bool = default;

 private:
  static constexpr std::uint8_t inward_bit = 1U;
  static constexpr std::uint8_t outward_bit = 2U;

  constexpr explicit FrameResizeDirections(std::uint8_t bits) noexcept
      : bits_(bits) {}

  std::uint8_t bits_{0};
};

inline constexpr FrameResizeDirections FrameResizeDirections::inward{
    FrameResizeDirection::inward};
inline constexpr FrameResizeDirections FrameResizeDirections::outward{
    FrameResizeDirection::outward};
inline constexpr FrameResizeDirections FrameResizeDirections::all{
    FrameResizeDirections::inward | FrameResizeDirections::outward};

}
