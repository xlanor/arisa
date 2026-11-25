#include <interaction/drag_session.hpp>

#include <arisa/composition/drop_configuration.hpp>
#include <arisa/composition/drop_target.hpp>
#include <arisa/composition/reorder_item.hpp>
#include <arisa/composition/spring_loading_behavior.hpp>

#include <interaction/gesture_arena.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <utility>

namespace arisa::interaction {

auto DragSession::view() const -> std::optional<View> {
  if (!source_.has_value()) {
    return std::nullopt;
  }
  return View{.source = *source_,
              .location = location_,
              .size = size_,
              .target = target_,
              .revision = revision_,
              .count = std::max<std::size_t>(ids_.size(), 1),
              .formation = landing_.value_or(formation_),
              .operation = operation_};
}

auto DragSession::bounds(const composition::Tree& tree,
                         composition::NodeId node)
    -> std::optional<composition::Rect> {
  const auto found = tree.node(node);
  if (!found.has_value()) {
    return std::nullopt;
  }
  if (const auto& presented = found->get().presented(); presented.has_value()) {
    return presented->bounds;
  }
  return found->get().frame();
}

auto DragSession::centre(const composition::Tree& tree,
                         composition::NodeId node)
    -> std::optional<composition::Point> {
  return bounds(tree, node).transform([](const composition::Rect& rect) {
    return composition::Point{.x = rect.x + (rect.width * 0.5F),
                              .y = rect.y + (rect.height * 0.5F)};
  });
}

auto DragSession::ancestry(const composition::Tree& tree,
                           composition::NodeId node)
    -> std::vector<std::reference_wrapper<const composition::Node>> {
  std::vector<std::reference_wrapper<const composition::Node>> found;
  std::optional<composition::NodeId> current = node;
  while (current.has_value()) {
    const auto at = tree.node(*current);
    if (!at.has_value()) {
      break;
    }
    found.push_back(std::cref(at->get()));
    current = at->get().parent();
  }
  return found;
}

auto DragSession::springs(const composition::Tree& tree,
                          composition::NodeId node) -> bool {
  const auto found = tree.node(node);
  if (!found.has_value() || !found->get().content().activatable() ||
      found->get().interactions().disabled) {
    return false;
  }
  for (const composition::Node& each : ancestry(tree, node)) {
    if (const std::optional<composition::SpringLoadingBehavior>& behavior =
            each.interactions().spring_loading;
        behavior.has_value()) {
      return *behavior == composition::SpringLoadingBehavior::enabled;
    }
  }
  return false;
}

auto DragSession::begin(const composition::Tree& tree,
                        composition::NodeId source, Origin origin,
                        std::optional<composition::Point> location,
                        std::vector<Invocation>& invocations) -> bool {
  const auto found = tree.node(source);
  if (!found.has_value() || found->get().interactions().drag == nullptr) {
    return false;
  }
  const composition::DragSource& drag = *found->get().interactions().drag;
  const std::vector<std::reference_wrapper<const composition::Node>> chain =
      ancestry(tree, source);
  std::vector<composition::ElementKey> ids;
  std::size_t index = 0;
  if (drag.id().has_value()) {
    ids.push_back(*drag.id());
    for (const composition::Node& each : chain) {
      const auto& selection = each.interactions().drag_selection;
      if (selection == nullptr || !selection->serves(drag.container())) {
        continue;
      }
      std::vector<composition::ElementKey> selected = selection->selected();
      if (const auto at = std::ranges::find(selected, *drag.id());
          at != selected.end()) {
        index = static_cast<std::size_t>(at - selected.begin());
        ids = std::move(selected);
      }
      break;
    }
  }
  std::vector<composition::TransferItem> items;
  if (!ids.empty()) {
    for (const composition::Node& each : chain) {
      const auto& container = each.interactions().drag_container;
      if (container != nullptr && container->serves(drag.container())) {
        items = container->payload(ids);
        break;
      }
    }
  }
  if (items.empty()) {
    items.assign(drag.items().begin(), drag.items().end());
  }
  if (items.empty()) {
    return false;
  }
  configuration_ = composition::DragConfiguration{};
  formation_ = composition::DragDropPreviewsFormation::automatic;
  bool configured = false;
  bool formed = false;
  for (const composition::Node& each : chain) {
    if (!configured && each.interactions().drag_configuration.has_value()) {
      configuration_ = *each.interactions().drag_configuration;
      configured = true;
    }
    if (!formed && each.interactions().drag_previews.has_value()) {
      formation_ = *each.interactions().drag_previews;
      formed = true;
    }
  }
  const std::optional<composition::Rect> frame = bounds(tree, source);
  source_ = source;
  target_.reset();
  landing_.reset();
  reorder_.reset();
  spring_.reset();
  spring_since_.reset();
  sprung_ = false;
  items_ = std::move(items);
  ids_ = std::move(ids);
  index_ = index;
  origin_ = origin;
  operation_ = composition::DropOperation::copy;
  size_ = frame.has_value() ? composition::Size{.width = frame->width,
                                                .height = frame->height}
                            : composition::Size{};
  location_ =
      location.value_or(centre(tree, source).value_or(composition::Point{}));
  ++revision_;
  ++session_;
  notify_drag(tree, composition::DragSession::Phase::initial(), invocations);
  return true;
}

auto DragSession::local(const composition::Tree& tree,
                        composition::NodeId node) const -> composition::Point {
  const composition::Rect frame =
      bounds(tree, node).value_or(composition::Rect{});
  return composition::Point{.x = location_.x - frame.x,
                            .y = location_.y - frame.y};
}

auto DragSession::accepting(const composition::Tree& tree,
                            std::span<const composition::NodeId> chain) const
    -> std::optional<composition::NodeId> {
  for (const composition::NodeId id : chain) {
    if (id == source_) {
      continue;
    }
    const auto found = tree.node(id);
    if (!found.has_value() || found->get().interactions().disabled) {
      continue;
    }
    const std::shared_ptr<composition::DropTarget>& drop =
        found->get().interactions().drop;
    if (drop != nullptr && drop->accepts(items_, local(tree, id))) {
      return id;
    }
  }
  return std::nullopt;
}

void DragSession::collect(const composition::Node& node,
                          const composition::Tree& tree,
                          std::vector<Placed>& found) {
  if (node.phase() != composition::NodeLifecyclePhase::live) {
    return;
  }
  if (const std::optional<composition::ReorderItem>& item =
          node.interactions().reorder_item;
      item.has_value()) {
    found.push_back(
        Placed{.node = node.id(),
               .item = *item,
               .frame = bounds(tree, node.id()).value_or(composition::Rect{})});
    return;
  }
  for (const composition::Node& child : node.children()) {
    collect(child, tree, found);
  }
}

auto DragSession::reorder(const composition::Tree& tree,
                          composition::NodeId container,
                          std::optional<composition::NodeId> focused) const
    -> std::optional<composition::DropSession::ReorderTarget> {
  const auto found = tree.node(container);
  if (!found.has_value() || !found->get().interactions().reorder_container) {
    return std::nullopt;
  }
  std::vector<Placed> placed;
  for (const composition::Node& child : found->get().children()) {
    collect(child, tree, placed);
  }
  if (placed.empty()) {
    return composition::DropSession::ReorderTarget{.before = std::nullopt,
                                                   .collection = std::nullopt};
  }
  const auto distance = [this](const composition::Rect& frame) {
    const float dx = std::max(
        {frame.x - location_.x, 0.0F, location_.x - (frame.x + frame.width)});
    const float dy = std::max(
        {frame.y - location_.y, 0.0F, location_.y - (frame.y + frame.height)});
    return std::hypot(dx, dy);
  };
  const auto nearest = std::ranges::min_element(
      placed, {},
      [&distance](const Placed& each) { return distance(each.frame); });
  const std::optional<composition::ElementKey> collection =
      nearest->item.collection;
  std::vector<Placed> lane;
  for (Placed& each : placed) {
    if (each.item.collection == collection) {
      lane.push_back(std::move(each));
    }
  }
  const auto moving = [this](const Placed& each) {
    return std::ranges::contains(ids_, each.item.id);
  };
  if (focused.has_value()) {
    const auto at = std::ranges::find(lane, *focused, &Placed::node);
    const auto from = std::ranges::find_if(lane, [this](const Placed& each) {
      return ids_.size() > index_ && each.item.id == ids_[index_];
    });
    if (at != lane.end()) {
      auto next = at;
      if (from != lane.end() && from < at) {
        next = std::next(at);
      }
      next = std::find_if_not(next, lane.end(), moving);
      return composition::DropSession::ReorderTarget{
          .before = next != lane.end()
                        ? std::optional<composition::ElementKey>{next->item.id}
                        : std::nullopt,
          .collection = collection};
    }
  }
  bool column = true;
  if (lane.size() >= 2) {
    column = std::abs(lane[1].frame.y - lane[0].frame.y) >=
             std::abs(lane[1].frame.x - lane[0].frame.x);
  }
  for (const Placed& each : lane) {
    if (moving(each)) {
      continue;
    }
    const composition::Rect& frame = each.frame;
    const float middle_x = frame.x + (frame.width * 0.5F);
    const float middle_y = frame.y + (frame.height * 0.5F);
    bool before = false;
    if (location_.y < frame.y) {
      before = true;
    } else if (location_.y <= frame.y + frame.height) {
      before = column ? location_.y < middle_y : location_.x < middle_x;
    }
    if (before) {
      return composition::DropSession::ReorderTarget{.before = each.item.id,
                                                     .collection = collection};
    }
  }
  return composition::DropSession::ReorderTarget{.before = std::nullopt,
                                                 .collection = collection};
}

auto DragSession::snapshot(const composition::Tree& tree,
                           composition::NodeId target,
                           composition::DropSession::Phase phase) const
    -> composition::DropSession {
  return composition::DropSession{composition::DropSession::Snapshot{
      .id = drop_session_,
      .phase = phase,
      .local = composition::DropSession::LocalSession{ids_},
      .items_count = items_.size(),
      .suggested = configuration_.operations_within_app().allowed(),
      .size = size_,
      .location = local(tree, target),
      .reorder = target == target_ ? reorder_ : std::nullopt}};
}

auto DragSession::dragged(composition::DragSession::Phase phase) const
    -> composition::DragSession {
  return composition::DragSession{composition::DragSession::ID{session_}, phase,
                                  ids_, index_, location_};
}

void DragSession::notify_drag(const composition::Tree& tree,
                              composition::DragSession::Phase phase,
                              std::vector<Invocation>& invocations) const {
  if (!source_.has_value()) {
    return;
  }
  for (const composition::Node& each : ancestry(tree, *source_)) {
    if (const auto& updates = each.interactions().drag_updates;
        updates != nullptr) {
      invocations.push_back(Invocation::callback(
          [updates, session = dragged(phase)] { (*updates)(session); }));
      return;
    }
  }
}

void DragSession::notify_drop(const composition::Tree& tree,
                              composition::NodeId target,
                              composition::DropSession::Phase phase,
                              std::vector<Invocation>& invocations) const {
  for (const composition::Node& each : ancestry(tree, target)) {
    if (const auto& updates = each.interactions().drop_updates;
        updates != nullptr) {
      invocations.push_back(Invocation::callback(
          [updates, session = snapshot(tree, target, phase)] {
            (*updates)(session);
          }));
      return;
    }
  }
}

auto DragSession::resolve(const composition::Tree& tree)
    -> composition::DropOperation {
  if (!target_.has_value()) {
    return composition::DropOperation::cancel;
  }
  const auto found = tree.node(*target_);
  if (!found.has_value() || found->get().interactions().drop == nullptr) {
    return composition::DropOperation::cancel;
  }
  if (found->get().interactions().reorder_container) {
    return composition::DropOperation::move;
  }
  composition::DropOperation operation = composition::DropOperation::copy;
  const std::shared_ptr<composition::DropTarget> drop =
      found->get().interactions().drop;
  if (drop->delegate() != nullptr) {
    if (const std::optional<composition::DropProposal> proposal =
            drop->updated(items_, local(tree, *target_))) {
      operation = proposal->operation();
    }
  }
  for (const composition::Node& each : ancestry(tree, *target_)) {
    if (const auto& configure = each.interactions().drop_configuration;
        configure != nullptr) {
      operation =
          (*configure)(snapshot(tree, *target_,
                                composition::DropSession::Phase::active()))
              .operation();
      break;
    }
  }
  if (!configuration_.permits(operation)) {
    operation = composition::DropOperation::forbidden;
  }
  return operation;
}

void DragSession::retarget(const composition::Tree& tree,
                           std::optional<composition::NodeId> target,
                           std::vector<Invocation>& invocations) {
  if (target == target_) {
    return;
  }
  const auto drop_of =
      [&tree](
          composition::NodeId id) -> std::shared_ptr<composition::DropTarget> {
    const auto found = tree.node(id);
    return found.has_value() ? found->get().interactions().drop : nullptr;
  };
  if (target_.has_value()) {
    notify_drop(tree, *target_, composition::DropSession::Phase::exiting(),
                invocations);
    if (auto drop = drop_of(*target_)) {
      invocations.push_back(Invocation::callback(
          [drop, held = items_, at = local(tree, *target_)] {
            drop->exited(held, at);
            drop->targeted(false);
          }));
    }
  }
  target_ = target;
  landing_.reset();
  if (target.has_value()) {
    ++drop_session_;
    for (const composition::Node& each : ancestry(tree, *target)) {
      if (each.interactions().drop_previews.has_value()) {
        landing_ = each.interactions().drop_previews;
        break;
      }
    }
    notify_drop(tree, *target, composition::DropSession::Phase::entering(),
                invocations);
    if (auto drop = drop_of(*target)) {
      invocations.push_back(Invocation::callback(
          [drop, held = items_, at = local(tree, *target)] {
            drop->targeted(true);
            drop->entered(held, at);
          }));
    }
  }
  ++revision_;
}

void DragSession::respring(const composition::Tree& tree,
                           std::span<const composition::NodeId> chain) {
  std::optional<composition::NodeId> candidate;
  for (const composition::NodeId id : chain) {
    if (id != source_ && springs(tree, id)) {
      candidate = id;
      break;
    }
  }
  if (candidate != spring_) {
    spring_ = candidate;
    spring_since_.reset();
    sprung_ = false;
  }
}

void DragSession::update(const composition::Tree& tree,
                         std::span<const composition::NodeId> chain,
                         std::optional<composition::NodeId> focused,
                         std::vector<Invocation>& invocations) {
  retarget(tree, accepting(tree, chain), invocations);
  respring(tree, chain);
  reorder_ =
      target_.has_value() ? reorder(tree, *target_, focused) : std::nullopt;
  const composition::DropOperation operation = resolve(tree);
  if (operation != operation_) {
    operation_ = operation;
    ++revision_;
  }
  notify_drag(tree, composition::DragSession::Phase::active(), invocations);
  if (target_.has_value()) {
    notify_drop(tree, *target_, composition::DropSession::Phase::active(),
                invocations);
  }
}

void DragSession::move(const composition::Tree& tree,
                       composition::Point location,
                       std::vector<Invocation>& invocations) {
  if (!source_.has_value()) {
    return;
  }
  location_ = location;
  ++revision_;
  const std::vector<composition::NodeId> chain =
      GestureArena::chain(tree, location);
  update(tree, chain, std::nullopt, invocations);
}

void DragSession::follow(const composition::Tree& tree,
                         std::optional<composition::NodeId> focused,
                         std::vector<Invocation>& invocations) {
  if (!source_.has_value() || origin_ != Origin::controller) {
    return;
  }
  std::vector<composition::NodeId> chain;
  std::optional<composition::NodeId> current = focused;
  while (current.has_value()) {
    chain.push_back(*current);
    const auto found = tree.node(*current);
    current = found.has_value() ? found->get().parent() : std::nullopt;
  }
  bool moved = false;
  if (focused.has_value()) {
    if (const auto placed = centre(tree, *focused);
        placed.has_value() && *placed != location_) {
      location_ = *placed;
      ++revision_;
      moved = true;
    }
  }
  const std::optional<composition::NodeId> before = target_;
  if (!moved && accepting(tree, chain) == before) {
    return;
  }
  update(tree, chain, focused, invocations);
}

void DragSession::elapse(std::chrono::nanoseconds now,
                         std::vector<Invocation>& invocations) {
  if (!source_.has_value() || !spring_.has_value() || sprung_) {
    return;
  }
  if (!spring_since_.has_value()) {
    spring_since_ = now;
    return;
  }
  if (now - *spring_since_ >=
      std::chrono::nanoseconds{composition::SpringLoading::delay}) {
    sprung_ = true;
    invocations.push_back(Invocation::activation(*spring_));
  }
}

auto DragSession::drop(const composition::Tree& tree,
                       std::vector<Invocation>& invocations) -> bool {
  if (!source_.has_value()) {
    return false;
  }
  bool dropped = false;
  const composition::DropOperation operation = resolve(tree);
  if (target_.has_value() && operation != composition::DropOperation::cancel &&
      operation != composition::DropOperation::forbidden) {
    if (const auto found = tree.node(*target_);
        found.has_value() && found->get().interactions().drop != nullptr) {
      invocations.push_back(Invocation::callback(
          [drop = found->get().interactions().drop, items = items_,
           session =
               snapshot(tree, *target_,
                        composition::DropSession::Phase::ended(operation))] {
            static_cast<void>(drop->perform(items, session));
            drop->targeted(false);
          }));
      notify_drop(tree, *target_,
                  composition::DropSession::Phase::ended(operation),
                  invocations);
      notify_drop(tree, *target_,
                  composition::DropSession::Phase::data_transfer_completed(),
                  invocations);
      dropped = true;
    }
  } else {
    retarget(tree, std::nullopt, invocations);
  }
  const composition::DropOperation outcome =
      dropped ? operation : composition::DropOperation::cancel;
  notify_drag(tree, composition::DragSession::Phase::ended(outcome),
              invocations);
  if (dropped) {
    notify_drag(tree,
                composition::DragSession::Phase::data_transfer_completed(),
                invocations);
  }
  end();
  return dropped;
}

void DragSession::cancel(const composition::Tree& tree,
                         std::vector<Invocation>& invocations) {
  if (!source_.has_value()) {
    return;
  }
  retarget(tree, std::nullopt, invocations);
  notify_drag(tree,
              composition::DragSession::Phase::ended(
                  composition::DropOperation::cancel),
              invocations);
  end();
}

void DragSession::end() {
  source_.reset();
  target_.reset();
  items_.clear();
  ids_.clear();
  index_ = 0;
  landing_.reset();
  reorder_.reset();
  spring_.reset();
  spring_since_.reset();
  sprung_ = false;
  ++revision_;
}

}
