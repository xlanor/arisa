#include <arisa/components/grid_layout.hpp>
#include <components/stack_distribution.hpp>

#include <algorithm>
#include <vector>

namespace arisa::components {

auto GridLayout::size_that_fits(ProposedViewSize proposal,
                                const LayoutSubviews& subviews) const
    -> composition::Size {
  static_cast<void>(proposal);
  composition::Size total{};
  for (std::size_t index = 0; index < subviews.size(); ++index) {
    const composition::Size size =
        subviews[index].size_that_fits(ProposedViewSize::unspecified);
    total.width = std::max(total.width, size.width);
    total.height += size.height + row_gap(subviews, index);
  }
  return total;
}

void GridLayout::place_subviews(composition::Rect bounds,
                                ProposedViewSize proposal,
                                const LayoutSubviews& subviews) const {
  static_cast<void>(proposal);
  const composition::AlignmentKey key{options_.alignment.horizontal};
  std::vector<float> values;
  std::vector<float> widths;
  values.reserve(subviews.size());
  widths.reserve(subviews.size());
  float line = 0.0F;
  for (const LayoutSubview& subview : subviews) {
    const composition::ViewDimensions dimensions =
        subview.dimensions(ProposedViewSize::unspecified);
    values.push_back(dimensions[key]);
    widths.push_back(dimensions.width());
    line = std::max(line, values.back());
  }
  float column = 0.0F;
  for (std::size_t index = 0; index < subviews.size(); ++index) {
    column = std::max(column, line - values[index] + widths[index]);
  }
  const float left =
      bounds.x + ((bounds.width - column) * StackDistribution::fraction(key));
  float y = bounds.y;
  for (std::size_t index = 0; index < subviews.size(); ++index) {
    y += row_gap(subviews, index);
    subviews[index].place(
        composition::Point{.x = left + line - values[index], .y = y});
    y += subviews[index].size_that_fits(ProposedViewSize::unspecified).height;
  }
}

auto GridLayout::row_gap(const LayoutSubviews& subviews,
                         std::size_t index) const -> float {
  if (index == 0) {
    return 0.0F;
  }
  if (options_.vertical_spacing.has_value()) {
    return subviews[index].points(*options_.vertical_spacing);
  }
  return subviews[index - 1].spacing().distance(subviews[index].spacing(),
                                                composition::Axis::vertical);
}

}
