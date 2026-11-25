#include <arisa/components/any_layout.hpp>
#include <arisa/components/view_that_fits.hpp>

#include <memory>
#include <utility>

namespace arisa::components {

ViewThatFits::ViewThatFits(composition::Axes axes,
                           std::vector<composition::Element> children)
    : Element(
          std::make_unique<AnyLayout::Content>(
              std::make_shared<const AnyLayout::Typed<Fitting>>(Fitting{axes})),
          std::move(children)) {}

void ViewThatFits::conceal(const LayoutSubview& subview) { subview.hide(); }

auto ViewThatFits::Fitting::fits(composition::Size ideal,
                                 ProposedViewSize proposal) const -> bool {
  static constexpr float slack = 0.5F;
  const bool across = !axes_.contains(composition::Axis::horizontal) ||
                      !proposal.width.has_value() ||
                      ideal.width <= *proposal.width + slack;
  const bool down = !axes_.contains(composition::Axis::vertical) ||
                    !proposal.height.has_value() ||
                    ideal.height <= *proposal.height + slack;
  return across && down;
}

auto ViewThatFits::Fitting::choose(ProposedViewSize proposal,
                                   const LayoutSubviews& subviews) const
    -> std::size_t {
  const ProposedViewSize open{
      axes_.contains(composition::Axis::horizontal) ? std::nullopt
                                                    : proposal.width,
      axes_.contains(composition::Axis::vertical) ? std::nullopt
                                                  : proposal.height};
  for (std::size_t index = 0; index < subviews.size(); ++index) {
    if (fits(subviews[index].size_that_fits(open), proposal)) {
      return index;
    }
  }
  return subviews.empty() ? 0 : subviews.size() - 1;
}

auto ViewThatFits::Fitting::size_that_fits(ProposedViewSize proposal,
                                           const LayoutSubviews& subviews) const
    -> composition::Size {
  if (subviews.empty()) {
    return composition::Size{};
  }
  return subviews[choose(proposal, subviews)].size_that_fits(proposal);
}

void ViewThatFits::Fitting::place_subviews(
    composition::Rect bounds, ProposedViewSize,
    const LayoutSubviews& subviews) const {
  if (subviews.empty()) {
    return;
  }
  const ProposedViewSize offered{bounds.width, bounds.height};
  const std::size_t chosen = choose(offered, subviews);
  for (std::size_t index = 0; index < subviews.size(); ++index) {
    const LayoutSubview& subview = subviews[index];
    if (index == chosen) {
      subview.place(composition::Point{.x = bounds.x + (bounds.width * 0.5F),
                                       .y = bounds.y + (bounds.height * 0.5F)},
                    motion::UnitPoint::center, offered);
      continue;
    }
    conceal(subview);
    subview.place(composition::Point{.x = bounds.x, .y = bounds.y});
  }
}

}
