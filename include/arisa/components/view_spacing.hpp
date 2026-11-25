#pragma once

#include <arisa/composition/axis.hpp>
#include <arisa/theme/space.hpp>

#include <algorithm>
#include <array>
#include <cstddef>

namespace arisa::components {

class ViewSpacing {
 public:
  static const ViewSpacing zero;

  constexpr ViewSpacing() noexcept = default;

  constexpr ViewSpacing(float spacing, float text_spacing, bool text) noexcept {
    sides_.fill(Side{.spacing = spacing,
                     .text_spacing = text_spacing,
                     .text = text,
                     .rigid = false});
  }

  constexpr auto distance(const ViewSpacing& next,
                          composition::Axis axis) const noexcept -> float {
    const bool across = axis == composition::Axis::horizontal;
    const Side& mine = sides_[across ? trailing_side : bottom_side];
    const Side& theirs = next.sides_[across ? leading_side : top_side];
    if (mine.rigid || theirs.rigid) {
      return 0.0F;
    }
    if (!across && mine.text && theirs.text) {
      return std::max(mine.text_spacing, theirs.text_spacing);
    }
    return std::max(mine.spacing, theirs.spacing);
  }

  constexpr void form_union(const ViewSpacing& other,
                            theme::Edge edges = theme::Edge::all) noexcept {
    for (std::size_t side = 0; side < sides_.size(); ++side) {
      if ((static_cast<unsigned>(edges) & side_bits[side]) == 0U) {
        continue;
      }
      Side& mine = sides_[side];
      const Side& theirs = other.sides_[side];
      mine.spacing = std::max(mine.spacing, theirs.spacing);
      mine.text_spacing = std::max(mine.text_spacing, theirs.text_spacing);
      mine.text = mine.text && theirs.text;
      mine.rigid = mine.rigid && theirs.rigid;
    }
  }

  constexpr auto union_with(const ViewSpacing& other,
                            theme::Edge edges = theme::Edge::all) const noexcept
      -> ViewSpacing {
    ViewSpacing united = *this;
    united.form_union(other, edges);
    return united;
  }

  friend constexpr auto operator==(const ViewSpacing&, const ViewSpacing&)
      -> bool = default;

 private:
  struct Rigid {};

  struct Side {
    float spacing{8.0F};
    float text_spacing{0.0F};
    bool text{false};
    bool rigid{false};

    friend constexpr auto operator==(const Side&, const Side&)
        -> bool = default;
  };

  static constexpr std::size_t top_side = 0;
  static constexpr std::size_t leading_side = 1;
  static constexpr std::size_t bottom_side = 2;
  static constexpr std::size_t trailing_side = 3;
  static constexpr std::array<unsigned, 4> side_bits{
      static_cast<unsigned>(theme::Edge::top),
      static_cast<unsigned>(theme::Edge::leading),
      static_cast<unsigned>(theme::Edge::bottom),
      static_cast<unsigned>(theme::Edge::trailing)};

  constexpr explicit ViewSpacing(Rigid) noexcept {
    sides_.fill(Side{.rigid = true});
  }

  std::array<Side, 4> sides_{};
};

inline constexpr ViewSpacing ViewSpacing::zero{ViewSpacing::Rigid{}};

}
