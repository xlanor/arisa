#include <arisa/components/hstack_layout.hpp>
#include <components/stack_distribution.hpp>

namespace arisa::components {

auto HStackLayout::size_that_fits(ProposedViewSize proposal,
                                  const LayoutSubviews& subviews) const
    -> composition::Size {
  return StackDistribution{subviews, composition::Axis::horizontal,
                           options_.spacing}
      .size(proposal);
}

void HStackLayout::place_subviews(composition::Rect bounds,
                                  ProposedViewSize proposal,
                                  const LayoutSubviews& subviews) const {
  static_cast<void>(proposal);
  StackDistribution{subviews, composition::Axis::horizontal, options_.spacing}
      .place(bounds, options_.guide.value_or(
                         composition::AlignmentKey{options_.alignment}));
}

}
