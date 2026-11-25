#include <arisa/components/zstack_layout.hpp>
#include <components/stack_distribution.hpp>

#include <algorithm>
#include <vector>

namespace arisa::components {

auto ZStackLayout::size_that_fits(ProposedViewSize proposal,
                                  const LayoutSubviews& subviews) const
    -> composition::Size {
  composition::Size united{};
  for (const LayoutSubview& subview : subviews) {
    const composition::Size size = subview.size_that_fits(proposal);
    united.width = std::max(united.width, size.width);
    united.height = std::max(united.height, size.height);
  }
  return united;
}

void ZStackLayout::place_subviews(composition::Rect bounds,
                                  ProposedViewSize proposal,
                                  const LayoutSubviews& subviews) const {
  static_cast<void>(proposal);
  const ProposedViewSize offered{bounds.width, bounds.height};
  const composition::AlignmentKey across = options_.horizontal_guide.value_or(
      composition::AlignmentKey{options_.alignment.horizontal});
  const composition::AlignmentKey down = options_.vertical_guide.value_or(
      composition::AlignmentKey{options_.alignment.vertical});
  std::vector<composition::Point> values;
  std::vector<composition::Size> sizes;
  values.reserve(subviews.size());
  sizes.reserve(subviews.size());
  composition::Point line{};
  for (const LayoutSubview& subview : subviews) {
    const composition::ViewDimensions dimensions = subview.dimensions(offered);
    sizes.push_back(composition::Size{.width = dimensions.width(),
                                      .height = dimensions.height()});
    values.push_back(
        composition::Point{.x = dimensions[across], .y = dimensions[down]});
    line.x = std::max(line.x, values.back().x);
    line.y = std::max(line.y, values.back().y);
  }
  composition::Size extent{};
  for (std::size_t index = 0; index < subviews.size(); ++index) {
    extent.width =
        std::max(extent.width, line.x - values[index].x + sizes[index].width);
    extent.height =
        std::max(extent.height, line.y - values[index].y + sizes[index].height);
  }
  const float left = bounds.x + ((bounds.width - extent.width) *
                                 StackDistribution::fraction(across));
  const float top = bounds.y + ((bounds.height - extent.height) *
                                StackDistribution::fraction(down));
  for (std::size_t index = 0; index < subviews.size(); ++index) {
    subviews[index].place(
        composition::Point{.x = left + line.x - values[index].x,
                           .y = top + line.y - values[index].y},
        motion::UnitPoint::top_leading, offered);
  }
}

}
