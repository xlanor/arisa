#include <arisa/composition/paging_scroll_target_behavior.hpp>

#include <algorithm>
#include <cmath>

namespace arisa::composition {

void PagingScrollTargetBehavior::update_target(
    ScrollTarget& target, const ScrollTargetContext& context) const {
  if (context.axes.contains(Axis::horizontal)) {
    target.rect.x = paged(target.rect.x, context.starting_offset.x,
                          context.container_size.width);
  }
  if (context.axes.contains(Axis::vertical)) {
    target.rect.y = paged(target.rect.y, context.starting_offset.y,
                          context.container_size.height);
  }
  target.anchor.reset();
}

auto PagingScrollTargetBehavior::properties(
    const ScrollTargetBehaviorPropertiesContext& context) const
    -> ScrollTargetBehaviorProperties {
  static_cast<void>(context);
  return ScrollTargetBehaviorProperties{.limits_scrolls = true};
}

auto PagingScrollTargetBehavior::paged(float original, float starting,
                                       float page) -> float {
  if (page <= 0.0F) {
    return original;
  }
  const float start = std::round(starting / page);
  const float wanted =
      std::clamp(std::round(original / page), start - 1.0F, start + 1.0F);
  return wanted * page;
}

}
