#pragma once

#include <arisa/composition/horizontal_edge.hpp>

#include <cstdint>

namespace arisa::composition {

class HorizontalEdges {
 public:
  static const HorizontalEdges leading;
  static const HorizontalEdges trailing;
  static const HorizontalEdges all;

  constexpr HorizontalEdges() noexcept = default;

  constexpr HorizontalEdges(HorizontalEdge edge) noexcept
      : bits_(edge == HorizontalEdge::leading ? leading_bit : trailing_bit) {}

  constexpr auto contains(HorizontalEdges other) const noexcept -> bool {
    return other.bits_ != 0U && (bits_ & other.bits_) == other.bits_;
  }

  constexpr auto empty() const noexcept -> bool { return bits_ == 0U; }

  friend constexpr auto operator|(HorizontalEdges left,
                                  HorizontalEdges right) noexcept
      -> HorizontalEdges {
    return HorizontalEdges{static_cast<std::uint8_t>(left.bits_ | right.bits_)};
  }

  friend constexpr auto operator==(HorizontalEdges, HorizontalEdges)
      -> bool = default;

 private:
  static constexpr std::uint8_t leading_bit = 1U;
  static constexpr std::uint8_t trailing_bit = 2U;

  constexpr explicit HorizontalEdges(std::uint8_t bits) noexcept
      : bits_(bits) {}

  std::uint8_t bits_{0};
};

inline constexpr HorizontalEdges HorizontalEdges::leading{
    HorizontalEdge::leading};
inline constexpr HorizontalEdges HorizontalEdges::trailing{
    HorizontalEdge::trailing};
inline constexpr HorizontalEdges HorizontalEdges::all{
    HorizontalEdges::leading | HorizontalEdges::trailing};

}
