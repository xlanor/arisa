#include <components/stack_distribution.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <ranges>

namespace arisa::components {

StackDistribution::StackDistribution(const LayoutSubviews& subviews,
                                     composition::Axis axis,
                                     std::optional<theme::Space> spacing)
    : subviews_(subviews),
      axis_(axis),
      gaps_(subviews.size(), 0.0F),
      sizes_(subviews.size()) {
  for (std::size_t index = 1; index < subviews.size(); ++index) {
    gaps_[index] = spacing.has_value() ? subviews[index].points(*spacing)
                                       : subviews[index - 1].spacing().distance(
                                             subviews[index].spacing(), axis);
  }
}

auto StackDistribution::fraction(const composition::AlignmentKey& key) noexcept
    -> float {
  if (const auto horizontal = key.horizontal()) {
    switch (*horizontal) {
      case composition::HorizontalAlignment::leading:
        return 0.0F;
      case composition::HorizontalAlignment::center:
        return 0.5F;
      case composition::HorizontalAlignment::trailing:
        return 1.0F;
    }
  }
  if (const auto vertical = key.vertical()) {
    switch (*vertical) {
      case composition::VerticalAlignment::center:
        return 0.5F;
      case composition::VerticalAlignment::bottom:
        return 1.0F;
      case composition::VerticalAlignment::top:
      case composition::VerticalAlignment::first_text_baseline:
      case composition::VerticalAlignment::last_text_baseline:
        return 0.0F;
    }
  }
  return 0.0F;
}

auto StackDistribution::along(composition::Size size) const noexcept -> float {
  return axis_ == composition::Axis::horizontal ? size.width : size.height;
}

auto StackDistribution::across(composition::Size size) const noexcept -> float {
  return axis_ == composition::Axis::horizontal ? size.height : size.width;
}

auto StackDistribution::proposal_for(std::optional<float> main,
                                     std::optional<float> cross) const noexcept
    -> ProposedViewSize {
  return axis_ == composition::Axis::horizontal ? ProposedViewSize{main, cross}
                                                : ProposedViewSize{cross, main};
}

void StackDistribution::distribute(ProposedViewSize proposal) {
  const LayoutSubviews& subviews = subviews_;
  const std::optional<float> main = proposal.along(axis_);
  cross_ = proposal.along(axis_ == composition::Axis::horizontal
                              ? composition::Axis::vertical
                              : composition::Axis::horizontal);
  if (!main.has_value()) {
    for (std::size_t index = 0; index < subviews.size(); ++index) {
      sizes_[index] =
          subviews[index].size_that_fits(proposal_for(std::nullopt, cross_));
    }
    return;
  }
  const float gaps = std::reduce(gaps_.begin(), gaps_.end(), 0.0F);
  const float available = *main - gaps;
  std::vector<float> minima(subviews.size());
  std::vector<float> flexibility(subviews.size());
  std::vector<std::size_t> order(subviews.size());
  for (std::size_t index = 0; index < subviews.size(); ++index) {
    minima[index] =
        along(subviews[index].size_that_fits(proposal_for(0.0F, cross_)));
    const float widest = along(subviews[index].size_that_fits(
        proposal_for(std::numeric_limits<float>::infinity(), cross_)));
    flexibility[index] = widest - minima[index];
    order[index] = index;
  }
  std::ranges::stable_sort(order, [&](std::size_t left, std::size_t right) {
    const double first = subviews[left].priority();
    const double second = subviews[right].priority();
    if (first != second) {
      return first > second;
    }
    return flexibility[left] < flexibility[right];
  });
  float used = 0.0F;
  std::size_t at = 0;
  while (at < order.size()) {
    const double priority = subviews[order[at]].priority();
    std::size_t end = at;
    while (end < order.size() && subviews[order[end]].priority() == priority) {
      ++end;
    }
    float reserved = 0.0F;
    for (std::size_t index = 0; index < subviews.size(); ++index) {
      if (subviews[index].priority() < priority) {
        reserved += minima[index];
      }
    }
    float remaining = available - used - reserved;
    for (std::size_t step = at; step < end; ++step) {
      const std::size_t index = order[step];
      const auto left = static_cast<float>(end - step);
      const float offered = std::max(remaining / left, 0.0F);
      sizes_[index] = subviews[index].size_that_fits(
          proposal_for(std::isnan(offered) ? 0.0F : offered, cross_));
      remaining -= along(sizes_[index]);
      used += along(sizes_[index]);
    }
    at = end;
  }
}

auto StackDistribution::size(ProposedViewSize proposal) -> composition::Size {
  distribute(proposal);
  float main = std::reduce(gaps_.begin(), gaps_.end(), 0.0F);
  float cross = 0.0F;
  for (const composition::Size& size : sizes_) {
    main += along(size);
    cross = std::max(cross, across(size));
  }
  return axis_ == composition::Axis::horizontal
             ? composition::Size{.width = main, .height = cross}
             : composition::Size{.width = cross, .height = main};
}

void StackDistribution::place(composition::Rect bounds,
                              const composition::AlignmentKey& key) {
  const LayoutSubviews& subviews = subviews_;
  const bool across_horizontal = axis_ == composition::Axis::vertical;
  distribute(ProposedViewSize{bounds.width, bounds.height});
  std::vector<float> values(subviews.size());
  float line = 0.0F;
  for (std::size_t index = 0; index < subviews.size(); ++index) {
    values[index] = subviews[index].dimensions(
        proposal_for(along(sizes_[index]), cross_))[key];
    line = std::max(line, values[index]);
  }
  float extent = 0.0F;
  for (std::size_t index = 0; index < subviews.size(); ++index) {
    extent = std::max(extent, line - values[index] + across(sizes_[index]));
  }
  const float room = across_horizontal ? bounds.width : bounds.height;
  const float start = (across_horizontal ? bounds.x : bounds.y) +
                      ((room - extent) * fraction(key));
  float cursor = across_horizontal ? bounds.y : bounds.x;
  for (std::size_t index = 0; index < subviews.size(); ++index) {
    cursor += gaps_[index];
    const float offset = start + line - values[index];
    const composition::Point point =
        across_horizontal ? composition::Point{.x = offset, .y = cursor}
                          : composition::Point{.x = cursor, .y = offset};
    subviews[index].place(point, motion::UnitPoint::top_leading,
                          proposal_for(along(sizes_[index]), cross_));
    cursor += along(sizes_[index]);
  }
}

}
