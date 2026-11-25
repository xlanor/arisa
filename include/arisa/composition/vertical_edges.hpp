#pragma once

#include <arisa/composition/vertical_edge.hpp>

#include <cstdint>

namespace arisa::composition {

class VerticalEdges {
 public:
  static const VerticalEdges top;
  static const VerticalEdges bottom;
  static const VerticalEdges all;

  constexpr VerticalEdges() noexcept = default;

  constexpr VerticalEdges(VerticalEdge edge) noexcept
      : bits_(edge == VerticalEdge::top ? 1U : 2U) {}

  constexpr auto contains(VerticalEdges other) const noexcept -> bool {
    return (bits_ & other.bits_) == other.bits_ && other.bits_ != 0;
  }

  friend constexpr auto operator|(VerticalEdges left,
                                  VerticalEdges right) noexcept
      -> VerticalEdges {
    return VerticalEdges{static_cast<std::uint8_t>(left.bits_ | right.bits_)};
  }

  friend constexpr auto operator==(VerticalEdges, VerticalEdges)
      -> bool = default;

 private:
  constexpr explicit VerticalEdges(std::uint8_t bits) noexcept : bits_(bits) {}

  std::uint8_t bits_{0};
};

inline constexpr VerticalEdges VerticalEdges::top{1U};
inline constexpr VerticalEdges VerticalEdges::bottom{2U};
inline constexpr VerticalEdges VerticalEdges::all{3U};

}
