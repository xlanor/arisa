#pragma once

#include <arisa/composition/edge_corner.hpp>

#include <cstdint>

namespace arisa::composition {

class EdgeCorners {
 public:
  static const EdgeCorners none;
  static const EdgeCorners top_leading;
  static const EdgeCorners top_trailing;
  static const EdgeCorners bottom_leading;
  static const EdgeCorners bottom_trailing;
  static const EdgeCorners all;
  static const EdgeCorners leading;
  static const EdgeCorners trailing;
  static const EdgeCorners bottom;
  static const EdgeCorners top;

  constexpr EdgeCorners() noexcept = default;

  constexpr EdgeCorners(EdgeCorner corner) noexcept
      : bits_(static_cast<std::uint8_t>(1U << static_cast<unsigned>(corner))) {}

  constexpr auto contains(EdgeCorner corner) const noexcept -> bool {
    return (bits_ & EdgeCorners{corner}.bits_) != 0U;
  }

  constexpr auto contains(EdgeCorners other) const noexcept -> bool {
    return (bits_ & other.bits_) == other.bits_;
  }

  constexpr auto empty() const noexcept -> bool { return bits_ == 0U; }

  constexpr auto raw_value() const noexcept -> std::uint8_t { return bits_; }

  friend constexpr auto operator|(EdgeCorners left, EdgeCorners right) noexcept
      -> EdgeCorners {
    return EdgeCorners{static_cast<std::uint8_t>(left.bits_ | right.bits_)};
  }

  friend constexpr auto operator&(EdgeCorners left, EdgeCorners right) noexcept
      -> EdgeCorners {
    return EdgeCorners{static_cast<std::uint8_t>(left.bits_ & right.bits_)};
  }

  friend constexpr auto operator==(EdgeCorners, EdgeCorners) -> bool = default;

 private:
  constexpr explicit EdgeCorners(std::uint8_t bits) noexcept : bits_(bits) {}

  std::uint8_t bits_{0};
};

inline constexpr EdgeCorners EdgeCorners::none{};
inline constexpr EdgeCorners EdgeCorners::top_leading{EdgeCorner::top_leading};
inline constexpr EdgeCorners EdgeCorners::top_trailing{
    EdgeCorner::top_trailing};
inline constexpr EdgeCorners EdgeCorners::bottom_leading{
    EdgeCorner::bottom_leading};
inline constexpr EdgeCorners EdgeCorners::bottom_trailing{
    EdgeCorner::bottom_trailing};
inline constexpr EdgeCorners EdgeCorners::all{
    EdgeCorners::top_leading | EdgeCorners::top_trailing |
    EdgeCorners::bottom_leading | EdgeCorners::bottom_trailing};
inline constexpr EdgeCorners EdgeCorners::leading{EdgeCorners::top_leading |
                                                  EdgeCorners::bottom_leading};
inline constexpr EdgeCorners EdgeCorners::trailing{
    EdgeCorners::top_trailing | EdgeCorners::bottom_trailing};
inline constexpr EdgeCorners EdgeCorners::bottom{EdgeCorners::bottom_leading |
                                                 EdgeCorners::bottom_trailing};
inline constexpr EdgeCorners EdgeCorners::top{EdgeCorners::top_leading |
                                              EdgeCorners::top_trailing};

}
