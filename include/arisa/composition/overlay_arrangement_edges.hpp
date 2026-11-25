#pragma once

#include <arisa/composition/horizontal_edge.hpp>
#include <arisa/composition/layout_value_key.hpp>
#include <arisa/composition/vertical_edge.hpp>

#include <optional>

namespace arisa::composition {

class OverlayArrangementEdges {
 public:
  static const LayoutValueKey<OverlayArrangementEdges> key;

  constexpr OverlayArrangementEdges() noexcept = default;

  constexpr auto vertical() const noexcept -> std::optional<VerticalEdge> {
    return vertical_;
  }

  constexpr auto horizontal() const noexcept -> std::optional<HorizontalEdge> {
    return horizontal_;
  }

  constexpr auto with(std::optional<VerticalEdge> edge) const noexcept
      -> OverlayArrangementEdges {
    OverlayArrangementEdges changed = *this;
    changed.vertical_ = edge;
    return changed;
  }

  constexpr auto with(std::optional<HorizontalEdge> edge) const noexcept
      -> OverlayArrangementEdges {
    OverlayArrangementEdges changed = *this;
    changed.horizontal_ = edge;
    return changed;
  }

  friend constexpr auto operator==(const OverlayArrangementEdges&,
                                   const OverlayArrangementEdges&)
      -> bool = default;

 private:
  std::optional<VerticalEdge> vertical_;
  std::optional<HorizontalEdge> horizontal_;
};

inline constexpr LayoutValueKey<OverlayArrangementEdges>
    OverlayArrangementEdges::key{"overlay_arrangement_edges",
                                 OverlayArrangementEdges{}};

}
