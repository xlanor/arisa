#pragma once

#include <arisa/components/grid.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/layout_subviews.hpp>
#include <arisa/components/proposed_view_size.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/composition/size.hpp>

#include <cstddef>
#include <utility>
#include <vector>

namespace arisa::components {

class GridLayout {
 public:
  constexpr GridLayout() noexcept = default;

  constexpr explicit GridLayout(GridOptions options) noexcept
      : options_(options) {}

  constexpr auto options() const noexcept -> const GridOptions& {
    return options_;
  }

  template <class... Children>
    requires(composition::ViewContent<Children> && ...)
  auto operator()(Children&&... children) const -> composition::Element {
    return (*this)(Group::members(Group(std::forward<Children>(children)...)));
  }

  auto operator()(std::vector<composition::Element> children) const
      -> composition::Element {
    return Grid(Grid::Switchable{}, options_, std::move(children));
  }

  auto size_that_fits(ProposedViewSize proposal,
                      const LayoutSubviews& subviews) const
      -> composition::Size;

  void place_subviews(composition::Rect bounds, ProposedViewSize proposal,
                      const LayoutSubviews& subviews) const;

  friend constexpr auto operator==(const GridLayout&, const GridLayout&)
      -> bool = default;

 private:
  auto row_gap(const LayoutSubviews& subviews, std::size_t index) const
      -> float;

  GridOptions options_{};
};

}
