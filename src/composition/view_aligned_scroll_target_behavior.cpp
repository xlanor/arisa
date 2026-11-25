#include <arisa/composition/view_aligned_scroll_target_behavior.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace arisa::composition {

auto ViewAlignedScrollTargetBehavior::resolved_limit(
    bool horizontally_compact) const noexcept -> LimitBehavior {
  if (limit_ != LimitBehavior::automatic) {
    return limit_;
  }
  return horizontally_compact ? LimitBehavior::always : LimitBehavior::never;
}

auto ViewAlignedScrollTargetBehavior::properties(
    const ScrollTargetBehaviorPropertiesContext& context) const
    -> ScrollTargetBehaviorProperties {
  return ScrollTargetBehaviorProperties{
      .limits_scrolls =
          resolved_limit(context.horizontally_compact) != LimitBehavior::never};
}

auto ViewAlignedScrollTargetBehavior::aligned(
    const ScrollTargetContext& context, std::size_t index, bool vertical) const
    -> double {
  const Rect& view = context.aligned_views[index];
  const double share =
      anchor_.has_value() ? (vertical ? anchor_->y : anchor_->x) : 0.0;
  const double start = vertical ? view.y : view.x;
  const double length = vertical ? view.height : view.width;
  const double area = (vertical ? context.container_size.height
                                : context.container_size.width) -
                      (2.0 * context.aligned_margin);
  return start + (share * length) - (share * area);
}

auto ViewAlignedScrollTargetBehavior::nearest(
    const ScrollTargetContext& context, double position, bool vertical) const
    -> std::size_t {
  std::size_t best = 0;
  double distance = std::numeric_limits<double>::max();
  for (std::size_t index = 0; index < context.aligned_views.size(); ++index) {
    const double gap = std::abs(aligned(context, index, vertical) - position);
    if (gap < distance) {
      distance = gap;
      best = index;
    }
  }
  return best;
}

void ViewAlignedScrollTargetBehavior::update_target(
    ScrollTarget& target, const ScrollTargetContext& context) const {
  if (context.aligned_views.empty()) {
    return;
  }
  constexpr double tolerance = 0.5;
  const bool vertical = context.axes.contains(Axis::vertical);
  const double projected = vertical ? target.rect.y : target.rect.x;
  const double position =
      vertical ? context.current_offset.y : context.current_offset.x;
  const double speed =
      vertical ? context.velocity.height : context.velocity.width;
  const std::size_t count = context.aligned_views.size();
  std::size_t chosen = nearest(context, projected, vertical);
  if (speed > fling_speed &&
      aligned(context, chosen, vertical) <= position + tolerance) {
    for (std::size_t index = 0; index < count; ++index) {
      if (aligned(context, index, vertical) > position + tolerance) {
        chosen = index;
        break;
      }
    }
  } else if (speed < -fling_speed &&
             aligned(context, chosen, vertical) >= position - tolerance) {
    for (std::size_t index = 0; index < count; ++index) {
      if (aligned(context, index, vertical) < position - tolerance) {
        chosen = index;
      }
    }
  }
  const LimitBehavior limit = resolved_limit(context.horizontally_compact);
  if (limit == LimitBehavior::always_by_one ||
      limit == LimitBehavior::always_by_few) {
    const std::size_t reach =
        limit == LimitBehavior::always_by_one ? 1 : few_views;
    const std::size_t origin = nearest(
        context,
        vertical ? context.starting_offset.y : context.starting_offset.x,
        vertical);
    const std::size_t lowest = origin > reach ? origin - reach : 0;
    const std::size_t highest = std::min(origin + reach, count - 1);
    chosen = std::clamp(chosen, lowest, highest);
  }
  const auto value = static_cast<float>(aligned(context, chosen, vertical));
  if (vertical) {
    target.rect.y = value;
  } else {
    target.rect.x = value;
  }
  target.anchor.reset();
}

}
