#include <interaction/scroll_coordinator.hpp>

#include <arisa/composition/layer.hpp>
#include <arisa/composition/node_lifecycle_phase.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <utility>

namespace arisa::interaction {

auto ScrollCoordinator::follow() -> motion::Animation {
  return motion::Animation::spring(0.4, 1.0);
}

void ScrollCoordinator::synchronise(const composition::Tree& tree) {
  const std::pair stamp{tree.revision(), tree.layouts()};
  if (stamp_ == stamp) {
    if (arranged_ != tree.arrangement()) {
      arranged_ = tree.arrangement();
      rearrange(tree);
    }
    return;
  }
  stamp_ = stamp;
  arranged_ = tree.arrangement();
  ++synchronisations_;
  for (auto& entry : containers_) {
    entry.second.seen = false;
  }
  lazies_.clear();
  watchers_.clear();
  Inherited root;
  root.compact = compact_;
  inherited_.clear();
  inherited_.push_back(std::move(root));
  Visitor visitor{*this, tree};
  walk_.run(tree.root(), Reach{}, visitor);
  inherited_.clear();
  std::erase_if(containers_,
                [](const auto& entry) { return !entry.second.seen; });
  std::erase_if(watched_, [this](const auto& entry) {
    return std::ranges::none_of(watchers_, [&entry](const Watcher& watcher) {
      return watcher.node == entry.first;
    });
  });
}

void ScrollCoordinator::rearrange(const composition::Tree& tree) {
  for (auto& [id, entry] : containers_) {
    const auto node = tree.node(id);
    if (!node.has_value()) {
      continue;
    }
    entry.viewport = node->get().frame();
    entry.visible = visible_of(node->get(), entry.viewport);
  }
}

auto ScrollCoordinator::declares(
    const composition::Scrolling& scrolling) noexcept -> bool {
  return scrolling.disabled.has_value() ||
         scrolling.target_behavior.has_value() ||
         scrolling.custom_target_behavior.has_value() ||
         !scrolling.indicators.empty() || !scrolling.bounce.empty() ||
         scrolling.deceleration.has_value() ||
         scrolling.dismisses_keyboard.has_value() ||
         !scrolling.inputs.empty() ||
         scrolling.anchors != composition::Scrolling::Anchors{} ||
         scrolling.flash.has_value() || scrolling.refresh != nullptr ||
         scrolling.scroll_position != nullptr ||
         scrolling.nested_geometry != nullptr;
}

auto ScrollCoordinator::inherit(const Inherited& outer,
                                const composition::Scrolling& scrolling)
    -> Inherited {
  Inherited inner = outer;
  if (scrolling.disabled.has_value()) {
    inner.disabled = outer.disabled.value_or(false) || *scrolling.disabled;
  }
  if (scrolling.target_behavior.has_value()) {
    inner.behavior = scrolling.target_behavior;
    inner.aligned = scrolling.aligned_behavior;
    inner.custom.reset();
  }
  if (scrolling.custom_target_behavior.has_value()) {
    inner.custom = scrolling.custom_target_behavior;
  }
  inner.indicators = scrolling.indicators.over(outer.indicators);
  inner.bounce = scrolling.bounce.over(outer.bounce);
  if (scrolling.deceleration.has_value()) {
    inner.rate = scrolling.deceleration;
  }
  if (scrolling.dismisses_keyboard.has_value()) {
    inner.keyboard = scrolling.dismisses_keyboard;
  }
  inner.inputs = scrolling.inputs.over(outer.inputs);
  inner.anchors = scrolling.anchors.over(outer.anchors);
  if (scrolling.flash.has_value()) {
    inner.flash = scrolling.flash;
  }
  if (scrolling.refresh != nullptr) {
    inner.refresh = scrolling.refresh;
  }
  if (scrolling.scroll_position != nullptr) {
    inner.placement = scrolling.scroll_position;
    inner.placement_anchor = scrolling.scroll_position_anchor;
  }
  if (scrolling.nested_geometry != nullptr) {
    inner.nested = scrolling.nested_geometry;
  }
  return inner;
}

auto ScrollCoordinator::consumed(const Inherited& settings,
                                 composition::Axes axes) -> Inherited {
  Inherited below = settings;
  below.disabled.reset();
  below.refresh = nullptr;
  below.placement = nullptr;
  below.nested = nullptr;
  if (axes.contains(composition::Axis::horizontal)) {
    below.indicators.horizontal.reset();
    below.bounce.horizontal.reset();
  }
  if (axes.contains(composition::Axis::vertical)) {
    below.indicators.vertical.reset();
    below.bounce.vertical.reset();
  }
  return below;
}

auto ScrollCoordinator::dismisses_on_scroll(const Container& container) noexcept
    -> bool {
  return container.keyboard ==
             composition::ScrollDismissesKeyboardMode::automatic ||
         container.keyboard ==
             composition::ScrollDismissesKeyboardMode::immediately;
}

auto ScrollCoordinator::bounces(
    std::optional<composition::ScrollBounceBehavior> behavior, float content,
    float viewport) noexcept -> bool {
  return behavior != composition::ScrollBounceBehavior::based_on_size ||
         content > viewport + movable_threshold;
}

void ScrollCoordinator::adopt(const composition::Tree& tree,
                              const composition::Node& node, Container& entry,
                              const Inherited& settings, bool created,
                              const motion::Transaction& transaction) {
  using composition::Axis;
  using composition::ScrollIndicatorVisibility;
  entry.viewport = node.frame();
  entry.insets = node.content_insets();
  entry.visible = visible_of(node, entry.viewport);
  entry.disabled = settings.disabled.value_or(false);
  entry.keyboard = settings.keyboard.value_or(
      composition::ScrollDismissesKeyboardMode::automatic);
  entry.inputs = settings.inputs;
  entry.seen = true;
  const composition::ScrollTargetBehavior behavior =
      settings.behavior.value_or(composition::ScrollTargetBehavior::standard);
  Scroller::Metrics& metrics = metrics_;
  metrics.viewport = composition::Size{.width = entry.viewport.width,
                                       .height = entry.viewport.height};
  entry.measured = node.content_size().has_value();
  metrics.content = node.content_size().value_or(metrics.viewport);
  metrics.axes = entry.axes;
  metrics.behavior = behavior;
  metrics.custom = settings.custom;
  metrics.environment = settings.custom.has_value()
                            ? tree.environment_values(node)
                            : composition::EnvironmentValues{};
  metrics.aligned =
      settings.aligned.value_or(composition::ViewAlignedScrollTargetBehavior{});
  metrics.horizontally_compact = settings.compact;
  metrics.rate =
      settings.rate.value_or(composition::ScrollDecelerationRate::normal())
          .value();
  metrics.bounces_horizontally =
      bounces(settings.bounce.horizontal, metrics.content.width,
              metrics.viewport.width);
  metrics.bounces_vertically =
      bounces(settings.bounce.vertical, metrics.content.height,
              metrics.viewport.height);
  entry.fitted = composition::Axes{};
  if (!metrics.bounces_horizontally) {
    entry.fitted = entry.fitted | composition::Axes{Axis::horizontal};
  }
  if (!metrics.bounces_vertically) {
    entry.fitted = entry.fitted | composition::Axes{Axis::vertical};
  }
  metrics.targets.clear();
  metrics.extents.clear();
  metrics.margin = 0.0F;
  if (behavior == composition::ScrollTargetBehavior::view_aligned ||
      settings.custom.has_value()) {
    retarget(node, entry.axes, entry.viewport);
  }
  const auto hides = [](std::optional<ScrollIndicatorVisibility> visibility) {
    return visibility == ScrollIndicatorVisibility::hidden ||
           visibility == ScrollIndicatorVisibility::never;
  };
  entry.indicator_axes = composition::Axes{};
  if (!hides(settings.indicators.horizontal)) {
    entry.indicator_axes =
        entry.indicator_axes | composition::Axes{Axis::horizontal};
  }
  if (!hides(settings.indicators.vertical)) {
    entry.indicator_axes =
        entry.indicator_axes | composition::Axes{Axis::vertical};
  }
  entry.anchors = settings.anchors;
  const composition::Point before = entry.scroller.offset();
  entry.scroller.configure(metrics);
  if (entry.measured && !entry.anchored) {
    entry.anchored = true;
    if (const auto& anchor = entry.anchors.initial_offset) {
      const composition::Point limit = entry.scroller.limit();
      const composition::Point goal{.x = anchor->x * limit.x,
                                    .y = anchor->y * limit.y};
      entry.scroller.reposition(goal);
      if (goal.x > movable_threshold || goal.y > movable_threshold) {
        followed_ = node.id();
      }
    }
  } else if (entry.measured && entry.anchors.size_changes.has_value() &&
             entry.scroller.idle() &&
             (metrics.content != entry.content_seen ||
              metrics.viewport != entry.viewport_seen)) {
    const motion::UnitPoint& anchor = *entry.anchors.size_changes;
    entry.scroller.reposition(composition::Point{
        .x =
            before.x +
            (anchor.x * ((metrics.content.width - entry.content_seen.width) -
                         (metrics.viewport.width - entry.viewport_seen.width))),
        .y = before.y +
             (anchor.y *
              ((metrics.content.height - entry.content_seen.height) -
               (metrics.viewport.height - entry.viewport_seen.height)))});
  }
  if (entry.measured && entry.anchored) {
    steady(node, entry, transaction);
  }
  if (entry.measured) {
    entry.content_seen = metrics.content;
    entry.viewport_seen = metrics.viewport;
  }
  if (created) {
    entry.geometry = geometry_of(entry);
  }
  if (settings.flash.has_value()) {
    const composition::Scrolling::Flash& flash = *settings.flash;
    if (created) {
      entry.flashing = flash.on_appear;
      entry.flash_seen = flash.trigger;
    } else if (flash.trigger.has_value() && flash.trigger != entry.flash_seen) {
      entry.flash_seen = flash.trigger;
      entry.flashing = true;
    }
  }
  entry.refresh =
      entry.axes.contains(Axis::vertical) ? settings.refresh : nullptr;
  entry.placement = settings.placement;
  entry.placement_anchor = settings.placement_anchor;
  entry.nested =
      entry.axes.contains(Axis::vertical) ? settings.nested : nullptr;
}

auto ScrollCoordinator::Visitor::enter(const composition::Node& node,
                                       const Reach& above) -> Walk::Step {
  return coordinator_.get().visit(tree_, node, above);
}

void ScrollCoordinator::Visitor::leave(const composition::Node&, Reach& reach) {
  ScrollCoordinator& coordinator = coordinator_;
  for (std::size_t popped = 0; popped < reach.pushed; ++popped) {
    coordinator.inherited_.pop_back();
  }
}

auto ScrollCoordinator::visit(const composition::Tree& tree,
                              const composition::Node& node, const Reach& above)
    -> Walk::Step {
  Walk::Step step{
      .state = Reach{.container = above.container, .below = above.below}};
  Reach& reach = step.state;
  if (node.parent().has_value() &&
      node.phase() != composition::NodeLifecyclePhase::live) {
    step.descend = false;
    return step;
  }
  const composition::Scrolling& scrolling = node.interactions().scrolling;
  if (declares(scrolling)) {
    inherited_.push_back(inherit(inherited_[above.below], scrolling));
    ++reach.pushed;
    reach.below = inherited_.size() - 1;
  }
  const std::size_t settings = reach.below;
  if (!scrolling.visibility_observers.empty() && above.container.has_value()) {
    watchers_.push_back(
        Watcher{.node = node.id(), .container = *above.container});
  }
  if (scrolling.container.has_value()) {
    const auto [found, created] = containers_.try_emplace(node.id());
    Container& entry = found->second;
    entry.axes = scrolling.container->axes;
    adopt(tree, node, entry, inherited_[settings], created,
          tree.transaction(node));
    Inherited passed = consumed(inherited_[settings], entry.axes);
    inherited_.push_back(std::move(passed));
    ++reach.pushed;
    reach.below = inherited_.size() - 1;
    reach.container = node.id();
  }
  if (node.content().reveals()) {
    lazies_.push_back(Lazy{.node = node.id(), .container = reach.container});
  }
  return step;
}

auto ScrollCoordinator::marked(const composition::Node& node)
    -> std::optional<std::reference_wrapper<const composition::Node>> {
  for (const composition::Node& child : node.children()) {
    if (child.phase() != composition::NodeLifecyclePhase::live) {
      continue;
    }
    if (child.interactions().scrolling.target_layout) {
      return std::cref(child);
    }
    if (child.interactions().scrolling.container.has_value()) {
      continue;
    }
    if (const auto deeper = marked(child)) {
      return deeper;
    }
  }
  return std::nullopt;
}

void ScrollCoordinator::gather(
    const composition::Node& node,
    std::vector<std::reference_wrapper<const composition::Node>>& found) {
  for (const composition::Node& child : node.children()) {
    if (child.phase() != composition::NodeLifecyclePhase::live) {
      continue;
    }
    if (child.key().has_value()) {
      found.emplace_back(child);
    } else {
      gather(child, found);
    }
  }
}

void ScrollCoordinator::retarget(const composition::Node& node,
                                 composition::Axes axes,
                                 const composition::Rect& viewport) {
  std::optional<std::reference_wrapper<const composition::Node>> layout =
      marked(node);
  if (!layout.has_value()) {
    for (const composition::Node& child : node.children()) {
      if (child.phase() == composition::NodeLifecyclePhase::live) {
        layout = std::cref(child);
        break;
      }
    }
  }
  if (!layout.has_value()) {
    return;
  }
  std::vector<std::reference_wrapper<const composition::Node>>& items = items_;
  items.clear();
  gather(layout->get(), items);
  if (items.empty()) {
    for (const composition::Node& child : layout->get().children()) {
      if (child.phase() == composition::NodeLifecyclePhase::live) {
        items.emplace_back(child);
      }
    }
  }
  if (items.empty()) {
    return;
  }
  const bool vertical = axes.contains(composition::Axis::vertical);
  const float origin = vertical ? viewport.y : viewport.x;
  float inset = std::numeric_limits<float>::max();
  for (const auto& item : items) {
    const composition::Rect& frame = item.get().frame();
    inset = std::min(inset, (vertical ? frame.y : frame.x) - origin);
  }
  std::ranges::sort(items, {}, [vertical](const auto& item) {
    return start_of(item.get().frame(), vertical);
  });
  for (const auto& item : items) {
    const composition::Rect& frame = item.get().frame();
    metrics_.targets.push_back(start_of(frame, vertical) - origin - inset);
    metrics_.extents.push_back(vertical ? frame.height : frame.width);
  }
  metrics_.margin = inset;
}

auto ScrollCoordinator::start_of(const composition::Rect& frame,
                                 bool vertical) noexcept -> float {
  return vertical ? frame.y : frame.x;
}

void ScrollCoordinator::steady(const composition::Node& node, Container& entry,
                               const motion::Transaction& transaction) {
  const bool vertical = entry.axes.contains(composition::Axis::vertical);
  const Scroller::Metrics& metrics = entry.scroller.metrics();
  const bool changed = metrics.content != entry.content_seen;
  if (changed && entry.steady_node.has_value() && entry.scroller.idle() &&
      !entry.anchors.size_changes.has_value() &&
      transaction.scroll_content_offset_adjustment_behavior() ==
          motion::ScrollContentOffsetAdjustmentBehavior::automatic) {
    for (const auto& item : items(node)) {
      if (item.get().id() != *entry.steady_node) {
        continue;
      }
      const float shift =
          start_of(item.get().frame(), vertical) - entry.steady_start;
      if (std::abs(shift) > movable_threshold) {
        composition::Point offset = entry.scroller.offset();
        if (vertical) {
          offset.y += shift;
        } else {
          offset.x += shift;
        }
        entry.scroller.reposition(offset);
      }
      break;
    }
  }
  remember(node, entry);
}

void ScrollCoordinator::remember(const composition::Node& node,
                                 Container& entry) {
  entry.steady_node.reset();
  const composition::Point offset = entry.scroller.offset();
  entry.steady_offset = offset;
  const bool vertical = entry.axes.contains(composition::Axis::vertical);
  if ((vertical ? offset.y : offset.x) <= movable_threshold) {
    return;
  }
  const composition::Rect visible = visible_rect(entry);
  const float edge = (vertical ? visible.y : visible.x) + 1.0F;
  for (const auto& item : items(node)) {
    const composition::Rect& frame = item.get().frame();
    const float end = vertical ? frame.y + frame.height : frame.x + frame.width;
    if (item.get().key().has_value() && end > edge) {
      entry.steady_node = item.get().id();
      entry.steady_start = start_of(frame, vertical);
      return;
    }
  }
}

auto ScrollCoordinator::scrollable(composition::NodeId node,
                                   composition::ScrollInputKind input) const
    -> std::optional<composition::Axes> {
  const auto found = containers_.find(node);
  if (found == containers_.end() || found->second.disabled ||
      found->second.axes.empty() || !found->second.inputs.accepts(input)) {
    return std::nullopt;
  }
  composition::Axes usable{};
  for (const composition::Axis axis :
       {composition::Axis::horizontal, composition::Axis::vertical}) {
    if (found->second.axes.contains(axis) &&
        !found->second.fitted.contains(axis)) {
      usable = usable | composition::Axes{axis};
    }
  }
  if (usable.empty()) {
    return std::nullopt;
  }
  return usable;
}

auto ScrollCoordinator::scroller(composition::NodeId node) const
    -> std::optional<std::reference_wrapper<const Scroller>> {
  const auto found = containers_.find(node);
  if (found == containers_.end()) {
    return std::nullopt;
  }
  return std::cref(found->second.scroller);
}

void ScrollCoordinator::grab(composition::NodeId node,
                             std::chrono::nanoseconds now) {
  if (const auto found = containers_.find(node); found != containers_.end()) {
    found->second.scroller.grab(now);
    found->second.dragged = composition::Size{};
    if (dismisses_on_scroll(found->second)) {
      dismissing_ = true;
    }
  }
}

void ScrollCoordinator::drag(composition::NodeId node,
                             composition::Size translation,
                             std::chrono::nanoseconds now) {
  if (const auto found = containers_.find(node); found != containers_.end()) {
    found->second.scroller.drag(translation, now);
    found->second.dragged = translation;
  }
}

void ScrollCoordinator::release(composition::NodeId node,
                                composition::Size velocity,
                                std::chrono::nanoseconds now) {
  if (const auto found = containers_.find(node); found != containers_.end()) {
    found->second.scroller.release(velocity, now);
    if (found->second.keyboard ==
            composition::ScrollDismissesKeyboardMode::interactively &&
        found->second.dragged.height > movable_threshold) {
      dismissing_ = true;
    }
    found->second.dragged = composition::Size{};
  }
}

auto ScrollCoordinator::enclosing(const composition::Tree& tree,
                                  composition::NodeId node) const
    -> std::vector<composition::NodeId> {
  std::vector<composition::NodeId> found;
  each_enclosing(tree, node,
                 [&found](composition::NodeId id) { found.push_back(id); });
  return found;
}

auto ScrollCoordinator::scrollable_toward(
    const composition::Tree& tree, composition::NodeId node,
    composition::MoveCommandDirection direction) const
    -> std::optional<composition::NodeId> {
  using Direction = composition::MoveCommandDirection;
  const bool vertical =
      direction == Direction::up || direction == Direction::down;
  const bool backward =
      direction == Direction::up || direction == Direction::left;
  for (const composition::NodeId id : enclosing(tree, node)) {
    const Container& container = containers_.at(id);
    if (container.disabled ||
        !container.inputs.accepts(composition::ScrollInputKind::controller) ||
        !container.axes.contains(vertical ? composition::Axis::vertical
                                          : composition::Axis::horizontal)) {
      continue;
    }
    const Scroller::Metrics& metrics = container.scroller.metrics();
    const composition::Point offset = container.scroller.animating()
                                          ? container.scroller.target()
                                          : container.scroller.offset();
    const float position = vertical ? offset.y : offset.x;
    const float limit = vertical
                            ? metrics.content.height - metrics.viewport.height
                            : metrics.content.width - metrics.viewport.width;
    if (backward ? position > 0.5F : position < limit - 0.5F) {
      return id;
    }
  }
  return std::nullopt;
}

auto ScrollCoordinator::at_start(composition::NodeId node,
                                 composition::Axis axis) const -> bool {
  const auto found = containers_.find(node);
  if (found == containers_.end()) {
    return false;
  }
  const composition::Point offset = found->second.scroller.offset();
  return (axis == composition::Axis::vertical ? offset.y : offset.x) <=
         movable_threshold;
}

auto ScrollCoordinator::overscrolled(composition::NodeId node) const -> bool {
  const auto found = containers_.find(node);
  return found != containers_.end() &&
         found->second.scroller.offset().y < -movable_threshold;
}

auto ScrollCoordinator::step_toward(composition::NodeId container,
                                    composition::MoveCommandDirection direction,
                                    std::chrono::nanoseconds now) -> bool {
  const auto found = containers_.find(container);
  if (found == containers_.end() ||
      !found->second.inputs.accepts(composition::ScrollInputKind::controller)) {
    return false;
  }
  if (dismisses_on_scroll(found->second)) {
    dismissing_ = true;
  }
  const composition::Rect& seen = found->second.visible;
  return nudge(container, direction,
               composition::Rect{.x = seen.x + (seen.width * 0.5F),
                                 .y = seen.y + (seen.height * 0.5F),
                                 .width = 0.0F,
                                 .height = 0.0F},
               -std::max(seen.width, seen.height), now);
}

auto ScrollCoordinator::nudge(composition::NodeId container,
                              composition::MoveCommandDirection direction,
                              const composition::Rect& keep, float margin,
                              std::chrono::nanoseconds now) -> bool {
  using Direction = composition::MoveCommandDirection;
  const auto found = containers_.find(container);
  if (found == containers_.end()) {
    return false;
  }
  const Scroller::Metrics& metrics = found->second.scroller.metrics();
  const float step_y = metrics.viewport.height * 0.5F;
  const float step_x = metrics.viewport.width * 0.5F;
  const composition::Point offset = found->second.scroller.animating()
                                        ? found->second.scroller.target()
                                        : found->second.scroller.offset();
  composition::Point goal = offset;
  switch (direction) {
    case Direction::up:
      goal.y -= step_y;
      break;
    case Direction::down:
      goal.y += step_y;
      break;
    case Direction::left:
      goal.x -= step_x;
      break;
    case Direction::right:
      goal.x += step_x;
      break;
  }
  const composition::Rect& seen = found->second.visible;
  const float top = keep.y - seen.y;
  const float left = keep.x - seen.x;
  switch (direction) {
    case Direction::up:
      goal.y = std::max(goal.y, top + keep.height + margin - seen.height);
      break;
    case Direction::down:
      goal.y = std::min(goal.y, top - margin);
      break;
    case Direction::left:
      goal.x = std::max(goal.x, left + keep.width + margin - seen.width);
      break;
    case Direction::right:
      goal.x = std::min(goal.x, left - margin);
      break;
  }
  goal.x = std::clamp(
      goal.x, 0.0F,
      std::max(metrics.content.width - metrics.viewport.width, 0.0F));
  goal.y = std::clamp(
      goal.y, 0.0F,
      std::max(metrics.content.height - metrics.viewport.height, 0.0F));
  if (std::abs(goal.x - offset.x) < 0.5F &&
      std::abs(goal.y - offset.y) < 0.5F) {
    return false;
  }
  found->second.scroller.animate(goal, follow(), now);
  return true;
}

auto ScrollCoordinator::within(const composition::Tree& tree,
                               composition::NodeId node,
                               composition::NodeId ancestor) const -> bool {
  std::optional<composition::NodeId> current = node;
  while (current.has_value()) {
    if (*current == ancestor) {
      return true;
    }
    const auto entry = tree.node(*current);
    if (!entry.has_value()) {
      return false;
    }
    current = entry->get().parent();
  }
  return false;
}

auto ScrollCoordinator::desired(const Container& container,
                                const composition::Rect& rect,
                                std::optional<motion::UnitPoint> anchor,
                                float margin) const -> composition::Point {
  const Scroller& scroller = container.scroller;
  const composition::Point current = scroller.target();
  const composition::Point limit = scroller.limit();
  const composition::ScrollTargetBehavior behavior =
      scroller.metrics().behavior;
  const auto along = [&](float start, float length, float view, float now_at,
                         float upper, float anchor_at, bool primary) -> float {
    float wanted = now_at;
    if (anchor.has_value()) {
      wanted = start + (anchor_at * length) - (anchor_at * view);
    } else {
      const float lower_bound = start + length + margin - view;
      const float upper_bound = start - margin;
      if (length + (2.0F * margin) > view) {
        wanted = upper_bound;
      } else {
        wanted = std::clamp(now_at, lower_bound, upper_bound);
      }
      if (primary && std::abs(wanted - now_at) > 0.5F) {
        if (behavior == composition::ScrollTargetBehavior::paging &&
            view > 0.0F) {
          wanted = std::floor(start / view) * view;
        } else if (behavior ==
                   composition::ScrollTargetBehavior::view_aligned) {
          const std::vector<float>& aligned = scroller.metrics().targets;
          std::optional<float> best;
          for (const float target : aligned) {
            const bool fits = start - target >= -0.5F &&
                              start + length - target <= view + 0.5F;
            if (fits && (!best.has_value() || std::abs(target - wanted) <
                                                  std::abs(*best - wanted))) {
              best = target;
            }
          }
          if (best.has_value()) {
            wanted = *best;
          }
        }
      }
    }
    return std::clamp(wanted, 0.0F, upper);
  };
  composition::Point result = current;
  const bool vertical = container.axes.contains(composition::Axis::vertical);
  if (container.axes.contains(composition::Axis::horizontal)) {
    result.x = along(rect.x - container.visible.x, rect.width,
                     container.visible.width, current.x, limit.x,
                     anchor.has_value() ? anchor->x : 0.0F, !vertical);
  }
  if (vertical) {
    result.y = along(rect.y - container.visible.y, rect.height,
                     container.visible.height, current.y, limit.y,
                     anchor.has_value() ? anchor->y : 0.0F, true);
  }
  return result;
}

auto ScrollCoordinator::bring(const composition::Tree& tree,
                              composition::NodeId from, composition::Rect rect,
                              std::optional<motion::UnitPoint> anchor,
                              float margin,
                              std::optional<motion::Animation> animation,
                              std::chrono::nanoseconds now) -> bool {
  bool moved = false;
  for (const composition::NodeId id : enclosing(tree, from)) {
    Container& container = containers_.at(id);
    if (container.scroller.dragging()) {
      return moved;
    }
    if (const auto node = tree.node(id)) {
      container.visible = visible_of(node->get(), container.viewport);
    }
    const composition::Point wanted = desired(container, rect, anchor, margin);
    const composition::Point current = container.scroller.target();
    if (std::abs(wanted.x - current.x) > 0.5F ||
        std::abs(wanted.y - current.y) > 0.5F) {
      container.scroller.animate(wanted, animation, now);
      moved = true;
    }
    rect.x -= wanted.x;
    rect.y -= wanted.y;
    const float left = std::max(rect.x, container.viewport.x);
    const float top = std::max(rect.y, container.viewport.y);
    const float right = std::min(
        rect.x + rect.width, container.viewport.x + container.viewport.width);
    const float bottom = std::min(
        rect.y + rect.height, container.viewport.y + container.viewport.height);
    if (right > left && bottom > top) {
      rect = composition::Rect{
          .x = left, .y = top, .width = right - left, .height = bottom - top};
    }
  }
  return moved;
}

auto ScrollCoordinator::reveal(const composition::Tree& tree,
                               composition::NodeId focused, float margin,
                               std::chrono::nanoseconds now) -> bool {
  const auto node = tree.node(focused);
  if (!node.has_value()) {
    return false;
  }
  return bring(tree, focused, node->get().frame(), std::nullopt, margin,
               follow(), now);
}

void ScrollCoordinator::requests(composition::Tree& tree,
                                 std::chrono::nanoseconds now, Report& report) {
  for (composition::ScrollRequest& request : tree.scroll_requests()->take()) {
    pending_.push_back(
        Pending{.request = std::move(request), .frames = request_frames});
  }
  std::erase_if(pending_, [&](Pending& pending) {
    const composition::ScrollRequest& request = pending.request;
    const std::optional<motion::Animation>& animation =
        request.transaction.animation();
    if (const auto found = tree.find(request.target, request.within)) {
      const auto node = tree.node(*found);
      if (bring(tree, *found, node->get().frame(), request.anchor, 0.0F,
                animation, now)) {
        report.changed = true;
      }
      return true;
    }
    for (const Lazy& lazy : lazies_) {
      if (!within(tree, lazy.node, request.within)) {
        continue;
      }
      if (const auto estimated = tree.locate(lazy.node, request.target)) {
        if (bring(tree, lazy.node, *estimated, request.anchor, 0.0F, animation,
                  now)) {
          report.changed = true;
        }
        report.changed = true;
        break;
      }
    }
    return --pending.frames <= 0;
  });
}

auto ScrollCoordinator::geometry_of(const Container& container)
    -> composition::ScrollGeometry {
  const composition::Point offset = container.scroller.offset();
  const Scroller::Metrics& metrics = container.scroller.metrics();
  const theme::Edges& insets = container.insets;
  const composition::Point origin{.x = offset.x - insets.leading,
                                  .y = offset.y - insets.top};
  return composition::ScrollGeometry{
      .content_offset = origin,
      .content_size =
          composition::Size{
              .width =
                  std::max(metrics.content.width - insets.horizontal(), 0.0F),
              .height =
                  std::max(metrics.content.height - insets.vertical(), 0.0F)},
      .content_insets = insets,
      .container_size =
          composition::Size{
              .width =
                  std::max(metrics.viewport.width - insets.horizontal(), 0.0F),
              .height =
                  std::max(metrics.viewport.height - insets.vertical(), 0.0F)},
      .visible_rect = composition::Rect{.x = origin.x,
                                        .y = origin.y,
                                        .width = metrics.viewport.width,
                                        .height = metrics.viewport.height}};
}

void ScrollCoordinator::step(composition::Tree& tree,
                             std::chrono::nanoseconds now, Report& report) {
  for (auto& [id, container] : containers_) {
    if (std::exchange(container.flashing, false)) {
      container.scroller.flash(now);
    }
    container.scroller.advance(now);
    const composition::Point shift = alignment_shift(container);
    const composition::Point scrolled = container.scroller.offset();
    const composition::ScrollPresentation presentation{
        .offset = composition::Point{.x = scrolled.x - shift.x,
                                     .y = scrolled.y - shift.y},
        .indicators = container.scroller.indicators(now),
        .indicator_axes = container.indicator_axes};
    tree.present_scroll(id, presentation);
    if (presentation != container.published || !container.scroller.idle()) {
      report.changed = true;
    }
    if (container.scroller.moving(now)) {
      report.changed = true;
    }
    container.published = presentation;
    const composition::ScrollGeometry geometry = geometry_of(container);
    const composition::ScrollPhase phase = container.scroller.phase(now);
    const auto node = tree.node(id);
    if (node.has_value()) {
      const composition::Scrolling& scrolling =
          node->get().interactions().scrolling;
      if (geometry != container.geometry) {
        for (const auto& observer : scrolling.geometry_observers) {
          if (observer->differs(container.geometry, geometry)) {
            report.invocations.push_back(Invocation::callback(
                [observer, before = container.geometry, after = geometry] {
                  observer->notify(before, after);
                }));
          }
        }
        if (container.nested != nullptr &&
            container.nested->differs(container.geometry, geometry)) {
          report.invocations.push_back(Invocation::callback(
              [observer = container.nested, before = container.geometry,
               after = geometry] { observer->notify(before, after); }));
        }
      }
      if (phase != container.phase && !scrolling.phase_actions.empty()) {
        const composition::ScrollPhaseChangeContext context =
            phase_context(container, geometry, phase);
        for (const auto& action : scrolling.phase_actions) {
          report.invocations.push_back(Invocation::callback(
              [action, before = container.phase, after = phase, context] {
                (*action)(before, after, context);
              }));
        }
      }
    }
    if (node.has_value() && phase == composition::ScrollPhase::idle &&
        container.measured && scrolled != container.steady_offset) {
      remember(node->get(), container);
    }
    const composition::ScrollPhase before = container.phase;
    container.geometry = geometry;
    container.phase = phase;
    observe(tree, id, container, before, phase, report);
    place(tree, id, container, phase, now, report);
  }
  for (const Watcher& watcher : watchers_) {
    const auto found = containers_.find(watcher.container);
    const auto node = tree.node(watcher.node);
    if (found == containers_.end() || !node.has_value()) {
      continue;
    }
    const double visible = share(
        node->get().frame(), visible_rect(found->second), found->second.axes);
    for (const auto& observer :
         node->get().interactions().scrolling.visibility_observers) {
      const bool shown = observer->threshold() <= 0.0
                             ? visible > 0.0
                             : visible >= observer->threshold() - 1.0e-6;
      bool& was = watched_[watcher.node];
      if (shown != was) {
        was = shown;
        report.invocations.push_back(Invocation::callback(
            [observer, shown] { observer->notify(shown); }));
      }
    }
  }
  for (const Lazy& lazy : lazies_) {
    composition::Rect visible = tree.root().frame();
    if (lazy.container.has_value()) {
      const auto found = containers_.find(*lazy.container);
      if (found != containers_.end()) {
        const Container& container = found->second;
        const composition::Point offset = container.scroller.animating()
                                              ? container.scroller.target()
                                              : container.scroller.offset();
        visible = composition::Rect{.x = container.viewport.x + offset.x,
                                    .y = container.viewport.y + offset.y,
                                    .width = container.viewport.width,
                                    .height = container.viewport.height};
      }
    }
    if (tree.reveal(lazy.node, visible)) {
      report.changed = true;
    }
  }
}

auto ScrollCoordinator::phase_context(
    const Container& container, const composition::ScrollGeometry& geometry,
    composition::ScrollPhase phase) -> composition::ScrollPhaseChangeContext {
  composition::ScrollPhaseChangeContext context{.geometry = geometry,
                                                .velocity = std::nullopt};
  if (phase == composition::ScrollPhase::interacting ||
      phase == composition::ScrollPhase::decelerating) {
    context.velocity = container.scroller.velocity();
  }
  return context;
}

auto ScrollCoordinator::visible_of(const composition::Node& node,
                                   const composition::Rect& viewport)
    -> composition::Rect {
  const theme::Edges& safe = node.safe_area();
  const composition::Rect inset{
      .x = viewport.x + safe.leading,
      .y = viewport.y + safe.top,
      .width = std::max(viewport.width - safe.horizontal(), 0.0F),
      .height = std::max(viewport.height - safe.vertical(), 0.0F)};
  if (!node.presented().has_value() || !node.presented()->clip.has_value()) {
    return inset;
  }
  const composition::Rect& clip = *node.presented()->clip;
  const float left = std::max(inset.x, clip.x);
  const float top = std::max(inset.y, clip.y);
  const float right = std::min(inset.x + inset.width, clip.x + clip.width);
  const float bottom = std::min(inset.y + inset.height, clip.y + clip.height);
  if (right <= left || bottom <= top) {
    return inset;
  }
  return composition::Rect{
      .x = left, .y = top, .width = right - left, .height = bottom - top};
}

auto ScrollCoordinator::visible_rect(const Container& container)
    -> composition::Rect {
  const composition::Point offset = container.scroller.offset();
  return composition::Rect{.x = container.viewport.x + offset.x,
                           .y = container.viewport.y + offset.y,
                           .width = container.viewport.width,
                           .height = container.viewport.height};
}

auto ScrollCoordinator::share(const composition::Rect& item,
                              const composition::Rect& visible,
                              composition::Axes axes) -> double {
  const bool vertical = axes.contains(composition::Axis::vertical);
  const float start = vertical ? item.y : item.x;
  const float extent = vertical ? item.height : item.width;
  const float low = vertical ? visible.y : visible.x;
  const float high = low + (vertical ? visible.height : visible.width);
  if (extent <= 0.0F) {
    return start >= low && start <= high ? 1.0 : 0.0;
  }
  const float overlap = std::min(start + extent, high) - std::max(start, low);
  return std::clamp(static_cast<double>(overlap / extent), 0.0, 1.0);
}

auto ScrollCoordinator::items(const composition::Node& node)
    -> std::span<const std::reference_wrapper<const composition::Node>> {
  std::optional<std::reference_wrapper<const composition::Node>> layout =
      marked(node);
  if (!layout.has_value()) {
    for (const composition::Node& child : node.children()) {
      if (child.phase() == composition::NodeLifecyclePhase::live) {
        layout = std::cref(child);
        break;
      }
    }
  }
  items_.clear();
  if (layout.has_value()) {
    gather(layout->get(), items_);
  }
  return items_;
}

auto ScrollCoordinator::alignment_shift(const Container& container)
    -> composition::Point {
  const std::optional<motion::UnitPoint>& anchor = container.anchors.alignment;
  composition::Point shift{};
  if (!anchor.has_value() || !container.measured) {
    return shift;
  }
  const Scroller::Metrics& metrics = container.scroller.metrics();
  if (container.axes.contains(composition::Axis::horizontal)) {
    shift.x = anchor->x *
              std::max(metrics.viewport.width - metrics.content.width, 0.0F);
  }
  if (container.axes.contains(composition::Axis::vertical)) {
    shift.y = anchor->y *
              std::max(metrics.viewport.height - metrics.content.height, 0.0F);
  }
  return shift;
}

auto ScrollCoordinator::top_key(const composition::Node& node,
                                const Container& container)
    -> std::optional<composition::ElementKey> {
  if (!marked(node).has_value()) {
    return std::nullopt;
  }
  const composition::Rect visible = visible_rect(container);
  const bool vertical = container.axes.contains(composition::Axis::vertical);
  for (const auto& item : items(node)) {
    const composition::Rect& frame = item.get().frame();
    const float end = vertical ? frame.y + frame.height : frame.x + frame.width;
    const float edge = (vertical ? visible.y : visible.x) + 1.0F;
    if (item.get().key().has_value() && end > edge) {
      return item.get().key();
    }
  }
  return std::nullopt;
}

void ScrollCoordinator::place(composition::Tree& tree, composition::NodeId id,
                              Container& container,
                              composition::ScrollPhase phase,
                              std::chrono::nanoseconds now, Report& report) {
  if (container.placement == nullptr || !container.measured) {
    return;
  }
  const composition::ScrollPosition wanted = container.placement->get();
  if (!container.placed.has_value() || *container.placed != wanted) {
    container.placed = wanted;
    container.touched = false;
    if (wanted.is_positioned_by_user() || wanted.is_automatic()) {
      return;
    }
    container.placing = true;
    report.changed = true;
    std::optional<motion::Animation> animation;
    std::optional<motion::UnitPoint> preferred = container.placement_anchor;
    bool preserves = false;
    if (const auto node = tree.node(id)) {
      const motion::Transaction& transaction = tree.transaction(node->get());
      animation = transaction.animation();
      preserves = transaction.scroll_position_update_preserves_velocity();
      if (const auto anchor = transaction.scroll_target_anchor()) {
        preferred = anchor;
      }
    }
    if (const auto view = wanted.view_id()) {
      pending_.push_back(
          Pending{.request =
                      composition::ScrollRequest{
                          .target = *view,
                          .anchor = wanted.anchor().value_or(
                              preferred.value_or(motion::UnitPoint::top)),
                          .transaction = motion::Transaction{animation},
                          .within = id},
                  .frames = request_frames});
      return;
    }
    const composition::Point limit = container.scroller.limit();
    composition::Point goal = container.scroller.offset();
    if (const auto edge = wanted.edge()) {
      switch (*edge) {
        case theme::Edge::top:
          goal.y = 0.0F;
          break;
        case theme::Edge::bottom:
          goal.y = limit.y;
          break;
        case theme::Edge::leading:
          goal.x = 0.0F;
          break;
        case theme::Edge::trailing:
          goal.x = limit.x;
          break;
        default:
          break;
      }
    } else if (const auto point = wanted.point()) {
      goal = *point;
    } else {
      goal.x = wanted.x().value_or(goal.x);
      goal.y = wanted.y().value_or(goal.y);
    }
    if (preserves && container.scroller.carry(goal, now)) {
      return;
    }
    container.scroller.animate(goal, animation, now);
    return;
  }
  if (phase != composition::ScrollPhase::idle) {
    if (!container.placing) {
      container.touched = true;
    }
    return;
  }
  const bool requested = std::ranges::any_of(
      pending_,
      [id](const Pending& each) { return each.request.within == id; });
  if (requested) {
    return;
  }
  container.placing = false;
  if (!std::exchange(container.touched, false)) {
    return;
  }
  const auto node = tree.node(id);
  if (!node.has_value()) {
    return;
  }
  composition::ScrollPosition written = composition::ScrollPosition::by_user(
      container.scroller.offset(), top_key(node->get(), container));
  if (written == *container.placed) {
    return;
  }
  container.placed = written;
  report.invocations.push_back(Invocation::callback(
      [binding = container.placement, written = std::move(written)] {
        binding->set(written);
      }));
}

void ScrollCoordinator::observe(composition::Tree& tree, composition::NodeId id,
                                Container& container,
                                composition::ScrollPhase before,
                                composition::ScrollPhase after,
                                Report& report) {
  const composition::Point offset = container.scroller.offset();
  if (after == composition::ScrollPhase::interacting) {
    container.pulled = std::min(container.pulled, offset.y);
  } else if (before == composition::ScrollPhase::interacting) {
    if (container.refresh != nullptr && container.pulled <= -refresh_pull) {
      report.invocations.push_back(Invocation::callback(
          [refresh = container.refresh] { (*refresh)(); }));
    }
    container.pulled = 0.0F;
  }
  const auto node = tree.node(id);
  if (!node.has_value()) {
    return;
  }
  const composition::Scrolling& scrolling =
      node->get().interactions().scrolling;
  if (scrolling.target_visibility_observers.empty() &&
      scrolling.position == nullptr) {
    return;
  }
  const composition::Rect visible = visible_rect(container);
  const auto found = items(node->get());
  const auto& observers = scrolling.target_visibility_observers;
  container.reported.resize(observers.size());
  for (std::size_t index = 0; index < observers.size(); ++index) {
    const double threshold = observers[index]->threshold();
    const auto qualifies = [&](const composition::Node& item) {
      const double amount = share(item.frame(), visible, container.axes);
      return item.key().has_value() &&
             (threshold <= 0.0 ? amount > 0.0 : amount >= threshold - 1.0e-6);
    };
    const std::vector<composition::ElementKey>& held =
        container.reported[index];
    std::size_t matched = 0;
    bool same = true;
    for (const auto& item : found) {
      if (!qualifies(item.get())) {
        continue;
      }
      if (matched >= held.size() || held[matched] != *item.get().key()) {
        same = false;
        break;
      }
      ++matched;
    }
    if (same && matched == held.size()) {
      continue;
    }
    std::vector<composition::ElementKey> keys;
    for (const auto& item : found) {
      if (qualifies(item.get())) {
        keys.push_back(*item.get().key());
      }
    }
    container.reported[index] = keys;
    report.invocations.push_back(Invocation::callback(
        [observer = observers[index], keys = std::move(keys)] {
          observer->notify(keys);
        }));
  }
  if (scrolling.position == nullptr) {
    return;
  }
  const std::optional<composition::ElementKey> bound =
      scrolling.position->read();
  if (!container.position.has_value() || *container.position != bound) {
    container.position = bound;
    if (bound.has_value()) {
      const std::optional<motion::UnitPoint> preferred =
          tree.transaction(node->get()).scroll_target_anchor();
      pending_.push_back(Pending{
          .request =
              composition::ScrollRequest{
                  .target = *bound,
                  .anchor =
                      preferred.value_or(scrolling.position->anchor().value_or(
                          motion::UnitPoint::top)),
                  .transaction = motion::Transaction{std::nullopt},
                  .within = id},
          .frames = request_frames});
      report.changed = true;
    }
    return;
  }
  const bool requested = std::ranges::any_of(
      pending_,
      [id](const Pending& each) { return each.request.within == id; });
  if (requested || container.scroller.animating() ||
      after != composition::ScrollPhase::idle) {
    return;
  }
  const bool vertical = container.axes.contains(composition::Axis::vertical);
  std::optional<composition::ElementKey> top;
  for (const auto& item : found) {
    const composition::Rect& frame = item.get().frame();
    const float end = vertical ? frame.y + frame.height : frame.x + frame.width;
    const float edge = (vertical ? visible.y : visible.x) + 1.0F;
    if (item.get().key().has_value() && end > edge) {
      top = item.get().key();
      break;
    }
  }
  if (top.has_value() && top != bound) {
    container.position = top;
    report.invocations.push_back(Invocation::callback(
        [binding = scrolling.position, top] { binding->write(top); }));
  }
}

auto ScrollCoordinator::stick(const composition::Tree& tree,
                              std::optional<composition::NodeId> focused,
                              provider::Stick right, float speed,
                              std::chrono::nanoseconds now) -> bool {
  const std::optional<std::chrono::nanoseconds> previous = stick_clock_;
  stick_clock_ = now;
  if (!previous.has_value() || containers_.empty()) {
    return false;
  }
  const std::chrono::nanoseconds elapsed =
      std::min<std::chrono::nanoseconds>(now - *previous, stick_step_limit);
  const double seconds = std::chrono::duration<double>(elapsed).count();
  bool moved = false;
  const auto drive = [&](composition::Axis axis, float component) {
    if (std::abs(component) < stick_deadzone) {
      return;
    }
    const std::optional<composition::NodeId> chosen =
        choose(tree, focused, axis);
    if (!chosen.has_value()) {
      return;
    }
    const float shaped = std::clamp(component, -1.0F, 1.0F);
    const auto delta = static_cast<float>(shaped * speed * seconds);
    const bool shifted = containers_.at(*chosen).scroller.nudge(
        axis == composition::Axis::vertical
            ? composition::Point{.x = 0.0F, .y = delta}
            : composition::Point{.x = delta, .y = 0.0F},
        now);
    followed_ = chosen;
    if (!shifted) {
      return;
    }
    moved = true;
    if (dismisses_on_scroll(containers_.at(*chosen))) {
      dismissing_ = true;
    }
  };
  drive(composition::Axis::vertical, -right.y);
  drive(composition::Axis::horizontal, right.x);
  return moved;
}

auto ScrollCoordinator::choose(const composition::Tree& tree,
                               std::optional<composition::NodeId> focused,
                               composition::Axis axis) const
    -> std::optional<composition::NodeId> {
  const auto usable = [&](composition::NodeId id) {
    const Container& container = containers_.at(id);
    return !container.disabled &&
           container.inputs.accepts(composition::ScrollInputKind::controller) &&
           container.axes.contains(axis);
  };
  const auto movable = [&](composition::NodeId id) {
    const composition::Point limit = containers_.at(id).scroller.limit();
    return usable(id) &&
           (axis == composition::Axis::vertical ? limit.y : limit.x) >
               movable_threshold;
  };
  if (focused.has_value()) {
    std::optional<composition::NodeId> first_movable;
    std::optional<composition::NodeId> first_usable;
    each_enclosing(tree, *focused, [&](composition::NodeId id) {
      if (!first_movable.has_value() && movable(id)) {
        first_movable = id;
      }
      if (!first_usable.has_value() && usable(id)) {
        first_usable = id;
      }
    });
    if (first_movable.has_value()) {
      return first_movable;
    }
    if (first_usable.has_value()) {
      return first_usable;
    }
  }
  for (const auto& entry : containers_) {
    if (usable(entry.first)) {
      return entry.first;
    }
  }
  return std::nullopt;
}

auto ScrollCoordinator::row_stop(composition::Tree& tree,
                                 composition::NodeId container,
                                 const Container& entry, float wanted,
                                 float current, bool forward) const
    -> std::optional<float> {
  for (const Lazy& lazy : lazies_) {
    if (lazy.container != container) {
      continue;
    }
    const auto stop = tree.page_stop(lazy.node, entry.visible.y + wanted);
    if (!stop.has_value()) {
      continue;
    }
    const float snapped = *stop - entry.visible.y;
    const bool advances = forward ? snapped > current + movable_threshold
                                  : snapped < current - movable_threshold;
    if (advances) {
      return snapped;
    }
  }
  return std::nullopt;
}

auto ScrollCoordinator::page(composition::Tree& tree,
                             std::optional<composition::NodeId> focused,
                             Page kind, std::chrono::nanoseconds now) -> bool {
  const std::optional<composition::NodeId> chosen =
      choose(tree, focused, composition::Axis::vertical);
  if (!chosen.has_value()) {
    return false;
  }
  Container& container = containers_.at(*chosen);
  const composition::Point current = container.scroller.target();
  const float end = container.scroller.limit().y;
  const float step = container.viewport.height * page_share;
  float wanted = current.y;
  switch (kind) {
    case Page::up:
      wanted = current.y - step;
      break;
    case Page::down:
      wanted = current.y + step;
      break;
    case Page::start:
      wanted = 0.0F;
      break;
    case Page::end:
      wanted = end;
      break;
  }
  if ((kind == Page::up || kind == Page::down) && wanted > 0.0F &&
      wanted < end) {
    if (const auto stop = row_stop(tree, *chosen, container, wanted, current.y,
                                   kind == Page::down)) {
      wanted = *stop;
    }
  }
  wanted = std::clamp(wanted, 0.0F, std::max(end, 0.0F));
  if (std::abs(wanted - current.y) < 0.5F) {
    return false;
  }
  container.scroller.animate(composition::Point{.x = current.x, .y = wanted},
                             follow(), now);
  followed_ = chosen;
  if (dismisses_on_scroll(container)) {
    dismissing_ = true;
  }
  return true;
}

auto ScrollCoordinator::wheel(std::span<const composition::NodeId> chain,
                              float x, float y, std::chrono::nanoseconds now)
    -> bool {
  bool moved = false;
  const auto drive = [&](composition::Axis axis, float amount) {
    if (amount == 0.0F) {
      return;
    }
    for (const composition::NodeId id : chain) {
      const auto found = containers_.find(id);
      if (found == containers_.end() || found->second.disabled ||
          !found->second.inputs.accepts(
              composition::ScrollInputKind::pointer) ||
          !found->second.axes.contains(axis)) {
        continue;
      }
      if (dismisses_on_scroll(found->second)) {
        dismissing_ = true;
      }
      const float delta = -amount * wheel_step;
      found->second.scroller.nudge(
          axis == composition::Axis::vertical
              ? composition::Point{.x = 0.0F, .y = delta}
              : composition::Point{.x = delta, .y = 0.0F},
          now);
      moved = true;
      return;
    }
  };
  drive(composition::Axis::vertical, y);
  drive(composition::Axis::horizontal, -x);
  return moved;
}

}
