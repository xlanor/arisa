#pragma once

#include <arisa/components/layout_subviews.hpp>
#include <arisa/components/proposed_view_size.hpp>
#include <arisa/composition/axes.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/composition/size.hpp>

#include <cstddef>
#include <utility>
#include <vector>

namespace arisa::components {

class ViewThatFits : public composition::Element {
 public:
  template <class... Children>
    requires(composition::ViewContent<Children> && ...)
  explicit ViewThatFits(Children&&... children)
      : ViewThatFits(composition::Axes::all,
                     collect(std::forward<Children>(children)...)) {}

  template <class... Children>
    requires(composition::ViewContent<Children> && ...)
  explicit ViewThatFits(composition::Axes axes, Children&&... children)
      : ViewThatFits(axes, collect(std::forward<Children>(children)...)) {}

  ViewThatFits(composition::Axes axes,
               std::vector<composition::Element> children);

 private:
  class Fitting {
   public:
    explicit Fitting(composition::Axes axes) noexcept : axes_(axes) {}

    auto size_that_fits(ProposedViewSize proposal,
                        const LayoutSubviews& subviews) const
        -> composition::Size;

    void place_subviews(composition::Rect bounds, ProposedViewSize proposal,
                        const LayoutSubviews& subviews) const;

    friend auto operator==(const Fitting&, const Fitting&) -> bool = default;

   private:
    auto choose(ProposedViewSize proposal, const LayoutSubviews& subviews) const
        -> std::size_t;

    auto fits(composition::Size ideal, ProposedViewSize proposal) const -> bool;

    composition::Axes axes_;
  };

  static void conceal(const LayoutSubview& subview);
};

}
