#include <interaction/gesture_arena.hpp>

#include <arisa/composition/layer.hpp>
#include <arisa/composition/node_lifecycle_phase.hpp>
#include <arisa/composition/paint_order.hpp>
#include <arisa/motion/transaction_stack.hpp>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <ranges>
#include <tuple>
#include <utility>

namespace arisa::interaction {

auto GestureArena::reaches(const composition::ExclusiveScopes& exclusive,
                           composition::OverlayLayer layer) noexcept -> bool {
  return !exclusive.active() || exclusive.keeps(layer);
}

void GestureArena::search(const composition::Node& node,
                          composition::Point point, bool disabled,
                          bool reachable,
                          const composition::ExclusiveScopes& exclusive,
                          std::vector<Hit>& path, std::vector<Hit>& best,
                          std::vector<Deferred>& deferred) {
  Searcher searcher{point, exclusive, path, best, deferred};
  Search walk;
  walk.run(node, Probe{.blocked = disabled, .within = reachable}, searcher);
}

auto GestureArena::Searcher::enter(const composition::Node& node,
                                   const Probe& above) -> Search::Step {
  Search::Step step{.state = Probe{}, .descend = false};
  const bool top = !node.parent().has_value();
  if (!top && node.phase() != composition::NodeLifecyclePhase::live) {
    return step;
  }
  const std::optional<composition::PresentedFrame>& presented =
      node.presented();
  if (!top && presented.has_value() && !presented->visible) {
    return step;
  }
  if (!node.interactions().hit_testing) {
    return step;
  }
  std::vector<Hit>& path = path_;
  const bool blocked = above.blocked || node.interactions().disabled;
  path.push_back(Hit{.node = node.id(), .disabled = blocked});
  const auto own = node.content_as<composition::Layer::Content>();
  const bool see_through =
      own.has_value() &&
      own->get().input() == composition::OverlayInput::cooperative;
  if (top || (above.within && !see_through && presented.has_value() &&
              presented->hits(point_))) {
    best_.get() = path;
  }
  Probe& probe = step.state;
  probe.entered = true;
  probe.blocked = blocked;
  probe.within = above.within && (!presented.has_value() ||
                                  !presented->clip_shape.has_value() ||
                                  presented->clip_shape->contains(point_));
  if (composition::PaintOrder::reorders(node)) {
    probe.arranged = true;
    probe.base = order_.size();
    composition::PaintOrder::arrange(node, order_);
    probe.end = order_.size();
    probe.position = probe.base;
  }
  step.descend = true;
  return step;
}

auto GestureArena::Searcher::next(const composition::Node& node, Probe& probe)
    -> const composition::Node* {
  const auto children = node.children();
  while (probe.arranged ? probe.position < probe.end
                        : probe.position < std::ranges::size(children)) {
    const composition::Node& child =
        probe.arranged ? *order_[probe.position++] : children[probe.position++];
    const auto layer = child.content_as<composition::Layer::Content>();
    if (!layer.has_value()) {
      return &child;
    }
    if (layer->get().input() != composition::OverlayInput::passthrough &&
        reaches(exclusive_, layer->get().layer())) {
      std::vector<Deferred>& deferred = deferred_;
      deferred.push_back(Deferred{.node = child,
                                  .path = path_,
                                  .disabled = probe.blocked,
                                  .z = layer->get().layer().z(),
                                  .order = deferred.size()});
    }
  }
  if (probe.arranged) {
    order_.resize(probe.base);
    probe.arranged = false;
    probe.position = std::ranges::size(children);
  }
  return nullptr;
}

void GestureArena::Searcher::leave(const composition::Node&, Probe& probe) {
  if (probe.entered) {
    path_.get().pop_back();
  }
}

auto GestureArena::space_origin(const composition::Tree& tree,
                                const composition::Node& node,
                                const composition::CoordinateSpace& space)
    -> composition::Point {
  if (space.kind() == composition::CoordinateSpace::Kind::local) {
    return composition::Point{.x = node.frame().x, .y = node.frame().y};
  }
  if (space.kind() != composition::CoordinateSpace::Kind::named) {
    return composition::Point{};
  }
  std::optional<composition::NodeId> current = node.id();
  while (current.has_value()) {
    const auto found = tree.node(*current);
    if (!found.has_value()) {
      break;
    }
    const composition::Node& at = found->get();
    if (at.interactions().coordinate_space == space.name()) {
      return composition::Point{.x = at.frame().x, .y = at.frame().y};
    }
    current = at.parent();
  }
  return composition::Point{};
}

auto GestureArena::locate(const composition::Tree& tree,
                          composition::Point point) -> std::vector<Hit> {
  std::vector<Hit> path;
  std::vector<Hit> best;
  std::vector<Deferred> deferred;
  const composition::ExclusiveScopes& exclusive = tree.exclusive();
  search(tree.root(), point, false,
         reaches(exclusive, composition::OverlayLayer::content), exclusive,
         path, best, deferred);
  std::size_t order = deferred.size();
  while (!deferred.empty()) {
    const auto next = std::ranges::min_element(
        deferred, {},
        [](const Deferred& entry) { return std::pair{entry.z, entry.order}; });
    Deferred chosen = std::move(*next);
    deferred.erase(next);
    std::vector<Deferred> nested;
    search(chosen.node.get(), point, chosen.disabled, true, exclusive,
           chosen.path, best, nested);
    for (Deferred& inner : nested) {
      inner.order = order++;
      deferred.push_back(std::move(inner));
    }
  }
  return best;
}

auto GestureArena::chain(const composition::Tree& tree,
                         composition::Point point)
    -> std::vector<composition::NodeId> {
  const std::vector<Hit> best = locate(tree, point);
  std::vector<composition::NodeId> found;
  found.reserve(best.size());
  for (const Hit& hit : best | std::views::reverse) {
    found.push_back(hit.node);
  }
  return found;
}

auto GestureArena::signal(Recognizer& recognizer, const Event& event)
    -> RecognizerSignals {
  return std::visit(
      [&event](auto& active) -> RecognizerSignals {
        using Type = std::remove_cvref_t<decltype(active)>;
        if constexpr (std::same_as<Type, PinchRecognizer>) {
          if (event.kind == Event::Kind::move && event.second.has_value()) {
            return active.pair(event.point, *event.second, event.now);
          }
        }
        switch (event.kind) {
          case Event::Kind::down:
            return active.begin(event.point, event.now);
          case Event::Kind::move:
            return active.move(event.point, event.now);
          case Event::Kind::up:
            return active.end(event.point, event.now);
          case Event::Kind::elapse:
            return active.elapse(event.now);
        }
        return RecognizerSignals{};
      },
      recognizer);
}

auto GestureArena::possible(const Recognizer& recognizer) -> bool {
  return std::visit([](const auto& active) { return active.possible(); },
                    recognizer);
}

auto GestureArena::waiting(const Recognizer& recognizer) -> bool {
  return std::visit([](const auto& active) { return active.waiting(); },
                    recognizer);
}

auto GestureArena::single_finger(const Recognizer& recognizer) -> bool {
  return std::holds_alternative<TapRecognizer>(recognizer) ||
         std::holds_alternative<LongPressRecognizer>(recognizer) ||
         std::holds_alternative<LiftRecognizer>(recognizer);
}

auto GestureArena::recognizer_for(const composition::GestureLayout::Leaf& leaf)
    -> Recognizer {
  return std::visit(
      [](const auto& gesture) -> Recognizer {
        using Type = std::remove_cvref_t<decltype(gesture)>;
        if constexpr (std::same_as<Type, composition::TapGesture> ||
                      std::same_as<Type, composition::SpatialTapGesture>) {
          return TapRecognizer{gesture.count()};
        } else if constexpr (std::same_as<Type,
                                          composition::LongPressGesture>) {
          return LongPressRecognizer{
              std::chrono::duration_cast<std::chrono::nanoseconds>(
                  std::chrono::duration<double>(gesture.minimum_duration())),
              gesture.maximum_distance()};
        } else if constexpr (std::same_as<Type, composition::DragGesture>) {
          return DragRecognizer{gesture.minimum_distance()};
        } else if constexpr (std::same_as<Type, composition::MagnifyGesture>) {
          return PinchRecognizer{PinchRecognizer::Measure::scale,
                                 gesture.minimum_scale_delta()};
        } else {
          return PinchRecognizer{PinchRecognizer::Measure::angle,
                                 gesture.minimum_angle_delta().radians()};
        }
      },
      leaf);
}

auto GestureArena::kinds_of(const composition::GestureLayout::Leaf& leaf)
    -> composition::GestureInputKinds {
  return std::visit([](const auto& gesture) { return gesture.input_kinds(); },
                    leaf);
}

auto GestureArena::recognized() const -> std::optional<std::string> {
  if (!winner_.has_value() || contenders_[*winner_].kind != Kind::gesture ||
      groups_[contenders_[*winner_].group].name.empty()) {
    return std::nullopt;
  }
  return groups_[contenders_[*winner_].group].name;
}

auto GestureArena::space_for(const composition::GestureLayout::Leaf& leaf)
    -> composition::CoordinateSpace {
  return std::visit(
      [](const auto& gesture) -> composition::CoordinateSpace {
        using Type = std::remove_cvref_t<decltype(gesture)>;
        if constexpr (std::same_as<Type, composition::DragGesture> ||
                      std::same_as<Type, composition::SpatialTapGesture>) {
          return gesture.coordinate_space();
        } else {
          return composition::CoordinateSpace::local;
        }
      },
      leaf);
}

auto GestureArena::winner() const -> std::optional<composition::NodeId> {
  if (!winner_.has_value()) {
    return std::nullopt;
  }
  return contenders_[*winner_].node;
}

auto GestureArena::touched() const -> std::optional<composition::NodeId> {
  if (!touching_) {
    return std::nullopt;
  }
  if (winner_.has_value()) {
    const Contender& won = contenders_[*winner_];
    return won.kind == Kind::gesture ? std::optional{won.node} : std::nullopt;
  }
  const auto live =
      std::ranges::find_if(contenders_, [](const Contender& contender) {
        return contender.kind == Kind::gesture && !contender.lost &&
               contender.armed;
      });
  return live == contenders_.end() ? std::nullopt : std::optional{live->node};
}

void GestureArena::build(composition::Tree& tree, composition::Point point,
                         const ScrollCoordinator& scrolls,
                         std::optional<composition::NodeId> confined) {
  std::vector<Hit> best = locate(tree, point);
  if (confined.has_value()) {
    const auto trap = std::ranges::find(best, *confined, &Hit::node);
    best.erase(best.begin(), trap);
  }
  std::vector<bool> silenced(best.size(), false);
  {
    bool below = false;
    for (std::size_t at = 0; at < best.size(); ++at) {
      silenced[at] = below;
      const auto found = tree.node(best[at].node);
      if (!found.has_value() || below) {
        continue;
      }
      for (const composition::GestureAttachment& attached :
           found->get().interactions().gestures) {
        if ((attached.mask & composition::GestureMask::subviews) ==
            composition::GestureMask::none) {
          below = true;
        }
      }
    }
  }
  std::vector<Contender> built;
  std::vector<std::tuple<int, std::ptrdiff_t, std::size_t>> ranks;
  groups_.clear();
  const auto add = [&](Contender contender) {
    const auto depth = static_cast<std::ptrdiff_t>(contender.depth);
    int category = 1;
    std::ptrdiff_t order = depth;
    if (contender.priority == composition::GesturePriority::high) {
      category = 0;
      order = -depth;
    } else if (contender.priority ==
               composition::GesturePriority::simultaneous) {
      category = 2;
    }
    ranks.emplace_back(category, order, built.size());
    built.push_back(std::move(contender));
  };
  bool focus_added = false;
  bool lift_added = false;
  for (std::size_t index = 0; index < best.size(); ++index) {
    const std::size_t at = best.size() - 1 - index;
    const Hit& hit = best[at];
    if (hit.disabled) {
      continue;
    }
    const auto found = tree.node(hit.node);
    if (!found.has_value()) {
      continue;
    }
    const composition::Node& node = found->get();
    const composition::Point origin{.x = node.frame().x, .y = node.frame().y};
    const composition::Size extent{.width = node.frame().width,
                                   .height = node.frame().height};
    const composition::Interactions& declared = node.interactions();
    const bool quiet = silenced[at];
    for (std::size_t attachment = 0; attachment < declared.gestures.size();
         ++attachment) {
      const composition::GestureAttachment& attached =
          declared.gestures[attachment];
      if (quiet || (attached.mask & composition::GestureMask::gesture) ==
                       composition::GestureMask::none) {
        continue;
      }
      const composition::GestureLayout& layout = attached.gesture->layout();
      const std::size_t group = groups_.size();
      groups_.push_back(
          Group{.branches = layout.branches(),
                .members = std::vector<std::size_t>(layout.size()),
                .handoff = attached.handoff,
                .name = attached.name});
      for (std::size_t leaf = 0; leaf < layout.size(); ++leaf) {
        const composition::GestureLayout::Leaf& shape = layout.leaf(leaf);
        Contender contender{.kind = Kind::gesture,
                            .node = hit.node,
                            .attachment = attachment,
                            .leaf = leaf,
                            .group = group,
                            .priority = attached.priority,
                            .depth = index,
                            .recognizer = recognizer_for(shape),
                            .blockers = {},
                            .origin = origin,
                            .extent = extent,
                            .space = space_for(shape),
                            .anchor = {},
                            .start = point,
                            .deadline = {},
                            .armed = true,
                            .awaiting = false,
                            .waiting = false,
                            .fired = false,
                            .done = false,
                            .lost = false,
                            .succeeded = false};
        contender.origin = space_origin(tree, node, contender.space);
        if (!kinds_of(shape).contains(kind_)) {
          contender.armed = false;
          contender.done = true;
          contender.lost = true;
        }
        add(std::move(contender));
      }
    }
    const bool silent_activation = quiet;
    if (!silent_activation && node.content().activatable() &&
        declared.focusable.value_or(true)) {
      add(Contender{.kind = Kind::activation,
                    .node = hit.node,
                    .attachment = 0,
                    .leaf = 0,
                    .group = 0,
                    .priority = composition::GesturePriority::normal,
                    .depth = index,
                    .recognizer = TapRecognizer{},
                    .blockers = {},
                    .origin = origin,
                    .extent = extent,
                    .space = composition::CoordinateSpace::local,
                    .anchor = {},
                    .start = point,
                    .deadline = {},
                    .armed = true,
                    .awaiting = false,
                    .waiting = false,
                    .fired = false,
                    .done = false,
                    .lost = false,
                    .succeeded = false});
    }
    if (!focus_added && !node.content().activatable() &&
        declared.focusable.value_or(false) &&
        declared.focus_interactions.has_value() &&
        (*declared.focus_interactions & composition::FocusInteractions::edit) ==
            composition::FocusInteractions::edit) {
      focus_added = true;
      add(Contender{.kind = Kind::focus,
                    .node = hit.node,
                    .attachment = 0,
                    .leaf = 0,
                    .group = 0,
                    .priority = composition::GesturePriority::simultaneous,
                    .depth = index,
                    .recognizer = TapRecognizer{},
                    .blockers = {},
                    .origin = origin,
                    .extent = extent,
                    .space = composition::CoordinateSpace::local,
                    .anchor = {},
                    .start = point,
                    .deadline = {},
                    .armed = true,
                    .awaiting = false,
                    .waiting = false,
                    .fired = false,
                    .done = false,
                    .lost = false,
                    .succeeded = false});
    }
    if (!lift_added && declared.drag != nullptr) {
      lift_added = true;
      add(Contender{.kind = Kind::lift,
                    .node = hit.node,
                    .attachment = 0,
                    .leaf = 0,
                    .group = 0,
                    .priority = composition::GesturePriority::normal,
                    .depth = index,
                    .recognizer = LiftRecognizer{lift_duration, lift_distance},
                    .blockers = {},
                    .origin = origin,
                    .extent = extent,
                    .space = composition::CoordinateSpace::global,
                    .anchor = {},
                    .start = point,
                    .deadline = {},
                    .armed = true,
                    .awaiting = false,
                    .waiting = false,
                    .fired = false,
                    .done = false,
                    .lost = false,
                    .succeeded = false});
    }
    if (const auto axes = scrolls.scrollable(hit.node)) {
      add(Contender{.kind = Kind::scroll,
                    .node = hit.node,
                    .attachment = 0,
                    .leaf = 0,
                    .group = 0,
                    .priority = composition::GesturePriority::normal,
                    .depth = index,
                    .recognizer = DragRecognizer{scroll_slop, *axes},
                    .blockers = {},
                    .origin = origin,
                    .extent = extent,
                    .space = composition::CoordinateSpace::global,
                    .anchor = {},
                    .start = point,
                    .deadline = {},
                    .armed = true,
                    .awaiting = false,
                    .waiting = false,
                    .fired = false,
                    .done = false,
                    .lost = false,
                    .succeeded = false});
    }
  }
  std::ranges::stable_sort(ranks);
  contenders_.clear();
  contenders_.reserve(built.size());
  for (const auto& rank : ranks) {
    contenders_.push_back(std::move(built[std::get<2>(rank)]));
  }
  for (std::size_t index = 0; index < contenders_.size(); ++index) {
    const Contender& contender = contenders_[index];
    if (contender.kind == Kind::gesture) {
      groups_[contender.group].members[contender.leaf] = index;
    }
  }
  for (std::size_t index = 0; index < contenders_.size(); ++index) {
    Contender& contender = contenders_[index];
    contender.armed = !gated(index);
    if (contender.priority == composition::GesturePriority::simultaneous) {
      continue;
    }
    for (std::size_t earlier = 0; earlier < contenders_.size(); ++earlier) {
      if (earlier == index) {
        continue;
      }
      const Contender& other = contenders_[earlier];
      const bool high = earlier < index &&
                        other.priority == composition::GesturePriority::high &&
                        !allied(earlier, index);
      const bool exclusive =
          relation(earlier, index) ==
              composition::GestureLayout::Relation::exclusive &&
          other.leaf < contender.leaf;
      if (high || exclusive) {
        contender.blockers.push_back(earlier);
      }
    }
  }
  winner_.reset();
  pressed_.reset();
  for (const Contender& contender : contenders_) {
    if (contender.kind == Kind::activation) {
      pressed_ = contender.node;
      break;
    }
  }
}

auto GestureArena::relation(std::size_t first, std::size_t second) const
    -> std::optional<composition::GestureLayout::Relation> {
  const Contender& left = contenders_[first];
  const Contender& right = contenders_[second];
  if (left.kind != Kind::gesture || right.kind != Kind::gesture ||
      left.group != right.group) {
    return std::nullopt;
  }
  const composition::GestureLayout::Branch* chosen = nullptr;
  for (const composition::GestureLayout::Branch& each :
       groups_[left.group].branches) {
    const bool holds = each.begin <= left.leaf && left.leaf < each.end &&
                       each.begin <= right.leaf && right.leaf < each.end;
    if (holds && (chosen == nullptr ||
                  each.end - each.begin < chosen->end - chosen->begin)) {
      chosen = &each;
    }
  }
  if (chosen == nullptr) {
    return std::nullopt;
  }
  return chosen->relation;
}

auto GestureArena::allied(std::size_t first, std::size_t second) const -> bool {
  const auto kind = relation(first, second);
  return kind == composition::GestureLayout::Relation::simultaneous ||
         kind == composition::GestureLayout::Relation::sequenced;
}

auto GestureArena::ended(const Group& group, std::size_t begin,
                         std::size_t end) const -> bool {
  if (end - begin == 1) {
    return contenders_[group.members[begin]].succeeded;
  }
  const auto found = std::ranges::find_if(
      group.branches,
      [begin, end](const composition::GestureLayout::Branch& each) {
        return each.begin == begin && each.end == end;
      });
  if (found == group.branches.end()) {
    return std::ranges::any_of(
        std::span{group.members}.subspan(begin, end - begin),
        [this](std::size_t member) { return contenders_[member].succeeded; });
  }
  switch (found->relation) {
    case composition::GestureLayout::Relation::exclusive:
      return ended(group, begin, found->middle) ||
             ended(group, found->middle, end);
    case composition::GestureLayout::Relation::simultaneous:
      return (ended(group, begin, found->middle) ||
              ended(group, found->middle, end)) &&
             settled(group, begin, end);
    case composition::GestureLayout::Relation::sequenced:
      return ended(group, found->middle, end);
  }
  return false;
}

auto GestureArena::settled(const Group& group, std::size_t begin,
                           std::size_t end) const -> bool {
  return std::ranges::all_of(
      std::span{group.members}.subspan(begin, end - begin),
      [this](std::size_t member) {
        const Contender& contender = contenders_[member];
        return contender.done || !contender.fired;
      });
}

auto GestureArena::gated(std::size_t index) const -> bool {
  const Contender& contender = contenders_[index];
  if (contender.kind != Kind::gesture) {
    return false;
  }
  const Group& group = groups_[contender.group];
  return std::ranges::any_of(
      group.branches, [&](const composition::GestureLayout::Branch& each) {
        return each.relation ==
                   composition::GestureLayout::Relation::sequenced &&
               each.middle <= contender.leaf && contender.leaf < each.end &&
               !ended(group, each.begin, each.middle);
      });
}

auto GestureArena::stranded(std::size_t index) const -> bool {
  const Contender& contender = contenders_[index];
  const Group& group = groups_[contender.group];
  return std::ranges::any_of(
      group.branches, [&](const composition::GestureLayout::Branch& each) {
        if (each.relation != composition::GestureLayout::Relation::sequenced ||
            contender.leaf < each.middle || contender.leaf >= each.end) {
          return false;
        }
        const auto first = std::span{group.members}.subspan(
            each.begin, each.middle - each.begin);
        return std::ranges::all_of(first,
                                   [this](std::size_t member) {
                                     return contenders_[member].done;
                                   }) &&
               !ended(group, each.begin, each.middle);
      });
}

auto GestureArena::gesture_of(const composition::Tree& tree,
                              const Contender& contender) const
    -> std::shared_ptr<composition::Gesture> {
  const auto node = tree.node(contender.node);
  if (!node.has_value()) {
    return nullptr;
  }
  const auto& gestures = node->get().interactions().gestures;
  if (contender.attachment >= gestures.size()) {
    return nullptr;
  }
  const std::shared_ptr<composition::Gesture>& gesture =
      gestures[contender.attachment].gesture;
  if (contender.leaf >= gesture->size()) {
    return nullptr;
  }
  return gesture;
}

auto GestureArena::base(const Contender& contender) const
    -> composition::Point {
  const composition::CoordinateSpace::Kind kind = contender.space.kind();
  if (kind == composition::CoordinateSpace::Kind::local ||
      kind == composition::CoordinateSpace::Kind::named) {
    return contender.origin;
  }
  return composition::Point{};
}

auto GestureArena::value(const Contender& contender, const Event& event) const
    -> composition::DragGesture::Value {
  const composition::Size velocity = velocity_.velocity(event.now);
  const composition::Size translation{
      .width = event.point.x - contender.start.x,
      .height = event.point.y - contender.start.y};
  const auto factor =
      static_cast<float>(Scroller::projection(1.0, Scroller::normal_rate));
  const composition::Size predicted{
      .width = translation.width + (velocity.width * factor),
      .height = translation.height + (velocity.height * factor)};
  const composition::Point at = base(contender);
  const composition::Point start{.x = contender.start.x - at.x,
                                 .y = contender.start.y - at.y};
  return composition::DragGesture::Value{
      .time = event.now,
      .location = composition::Point{.x = event.point.x - at.x,
                                     .y = event.point.y - at.y},
      .start_location = start,
      .translation = translation,
      .velocity = velocity,
      .predicted_end_location =
          composition::Point{.x = start.x + predicted.width,
                             .y = start.y + predicted.height},
      .predicted_end_translation = predicted};
}

auto GestureArena::magnified(const Contender& contender) const
    -> composition::MagnifyGesture::Value {
  const PinchRecognizer::Sample& sample =
      std::get<PinchRecognizer>(contender.recognizer).sample();
  const composition::Point at = base(contender);
  return composition::MagnifyGesture::Value{
      .time = sample.time,
      .magnification = sample.magnification,
      .velocity = sample.scale_velocity,
      .start_anchor =
          motion::UnitPoint{.x = contender.extent.width > 0.0F
                                     ? (sample.anchor.x - contender.origin.x) /
                                           contender.extent.width
                                     : 0.5F,
                            .y = contender.extent.height > 0.0F
                                     ? (sample.anchor.y - contender.origin.y) /
                                           contender.extent.height
                                     : 0.5F},
      .start_location = composition::Point{.x = sample.anchor.x - at.x,
                                           .y = sample.anchor.y - at.y}};
}

auto GestureArena::rotated(const Contender& contender) const
    -> composition::RotateGesture::Value {
  const PinchRecognizer::Sample& sample =
      std::get<PinchRecognizer>(contender.recognizer).sample();
  const composition::MagnifyGesture::Value placed = magnified(contender);
  return composition::RotateGesture::Value{
      .time = sample.time,
      .rotation = motion::Angle::radians(sample.rotation),
      .velocity = motion::Angle::radians(sample.angle_velocity),
      .start_anchor = placed.start_anchor,
      .start_location = placed.start_location};
}

void GestureArena::press(composition::Tree& tree, std::size_t index,
                         bool pressing, Effects& effects) {
  const Contender& contender = contenders_[index];
  auto gesture = gesture_of(tree, contender);
  if (gesture == nullptr) {
    return;
  }
  effects.invocations.push_back(
      Invocation::callback([gesture, leaf = contender.leaf, pressing] {
        if (auto* held = std::get_if<composition::LongPressGesture>(
                &gesture->layout().leaf(leaf))) {
          held->pressing(pressing);
          if (pressing) {
            held->handlers().changed(true);
          }
        }
      }));
}

void GestureArena::lose(composition::Tree& tree, std::size_t index,
                        Effects& effects) {
  Contender& contender = contenders_[index];
  contender.done = true;
  contender.lost = true;
  contender.waiting = false;
  contender.awaiting = false;
  if (contender.kind == Kind::activation && pressed_ == contender.node) {
    pressed_.reset();
    effects.changed = true;
  }
  if (contender.kind != Kind::gesture) {
    return;
  }
  auto gesture = gesture_of(tree, contender);
  if (gesture == nullptr) {
    return;
  }
  const bool attentive =
      std::visit([](auto& each) { return each.handlers().attentive(); },
                 gesture->layout().leaf(contender.leaf));
  if (attentive ||
      std::holds_alternative<LongPressRecognizer>(contender.recognizer)) {
    effects.invocations.push_back(
        Invocation::callback([gesture, leaf = contender.leaf] {
          std::visit([](auto& each) { each.handlers().cancelled(); },
                     gesture->layout().leaf(leaf));
        }));
  }
}

void GestureArena::cancel(composition::Tree& tree, std::size_t index,
                          ScrollCoordinator& scrolls, Effects& effects) {
  Contender& contender = contenders_[index];
  if (contender.done) {
    return;
  }
  const bool held =
      std::holds_alternative<LongPressRecognizer>(contender.recognizer) &&
      std::get<LongPressRecognizer>(contender.recognizer).pressing();
  std::visit([](auto& active) { static_cast<void>(active.cancel()); },
             contender.recognizer);
  contender.done = true;
  contender.lost = true;
  contender.waiting = false;
  contender.awaiting = false;
  switch (contender.kind) {
    case Kind::activation:
      if (pressed_ == contender.node) {
        pressed_.reset();
        effects.changed = true;
      }
      break;
    case Kind::scroll:
      if (contender.fired) {
        scrolls.release(contender.node, composition::Size{}, last_now_);
        effects.changed = true;
      }
      break;
    case Kind::lift:
      if (contender.fired) {
        effects.lift_cancelled = true;
        effects.changed = true;
      }
      break;
    case Kind::focus:
      break;
    case Kind::gesture: {
      if (held) {
        press(tree, index, false, effects);
      }
      if (auto gesture = gesture_of(tree, contender)) {
        effects.invocations.push_back(
            Invocation::callback([gesture, leaf = contender.leaf] {
              std::visit([](auto& each) { each.handlers().cancelled(); },
                         gesture->layout().leaf(leaf));
            }));
      }
      break;
    }
  }
}

auto GestureArena::continuous() noexcept -> motion::Transaction {
  motion::Transaction made;
  made.set_is_continuous(true);
  return made;
}

void GestureArena::notify_changed(composition::Tree& tree, std::size_t index,
                                  const Event& event, Effects& effects) {
  const Contender& contender = contenders_[index];
  auto gesture = gesture_of(tree, contender);
  if (gesture == nullptr) {
    return;
  }
  const composition::GestureLayout::Leaf& leaf =
      gesture->layout().leaf(contender.leaf);
  if (std::holds_alternative<composition::DragGesture>(leaf)) {
    effects.invocations.push_back(Invocation::callback(
        [gesture, at = contender.leaf, moved = value(contender, event)] {
          const motion::TransactionStack::Scope scope{continuous()};
          if (auto* drag = std::get_if<composition::DragGesture>(
                  &gesture->layout().leaf(at))) {
            drag->handlers().changed(moved);
          }
        }));
  } else if (std::holds_alternative<composition::MagnifyGesture>(leaf) &&
             std::holds_alternative<PinchRecognizer>(contender.recognizer)) {
    effects.invocations.push_back(Invocation::callback(
        [gesture, at = contender.leaf, scaled = magnified(contender)] {
          const motion::TransactionStack::Scope scope{continuous()};
          if (auto* magnify = std::get_if<composition::MagnifyGesture>(
                  &gesture->layout().leaf(at))) {
            magnify->handlers().changed(scaled);
          }
        }));
  } else if (std::holds_alternative<composition::RotateGesture>(leaf) &&
             std::holds_alternative<PinchRecognizer>(contender.recognizer)) {
    effects.invocations.push_back(Invocation::callback(
        [gesture, at = contender.leaf, turned = rotated(contender)] {
          const motion::TransactionStack::Scope scope{continuous()};
          if (auto* rotate = std::get_if<composition::RotateGesture>(
                  &gesture->layout().leaf(at))) {
            rotate->handlers().changed(turned);
          }
        }));
  }
}

void GestureArena::notify_ended(composition::Tree& tree, std::size_t index,
                                const Event& event, Effects& effects) {
  Contender& contender = contenders_[index];
  contender.succeeded = true;
  auto gesture = gesture_of(tree, contender);
  if (gesture == nullptr) {
    return;
  }
  const composition::GestureLayout::Leaf& leaf =
      gesture->layout().leaf(contender.leaf);
  const composition::Point at = base(contender);
  if (std::holds_alternative<composition::TapGesture>(leaf)) {
    effects.invocations.push_back(
        Invocation::callback([gesture, place = contender.leaf] {
          if (auto* tap = std::get_if<composition::TapGesture>(
                  &gesture->layout().leaf(place))) {
            tap->handlers().changed(std::monostate{});
            tap->handlers().ended(std::monostate{});
          }
        }));
  } else if (std::holds_alternative<composition::SpatialTapGesture>(leaf)) {
    const composition::SpatialTapGesture::Value tapped{
        .location = composition::Point{.x = event.point.x - at.x,
                                       .y = event.point.y - at.y}};
    effects.invocations.push_back(
        Invocation::callback([gesture, place = contender.leaf, tapped] {
          if (auto* tap = std::get_if<composition::SpatialTapGesture>(
                  &gesture->layout().leaf(place))) {
            tap->handlers().changed(tapped);
            tap->handlers().ended(tapped);
          }
        }));
  } else if (std::holds_alternative<composition::LongPressGesture>(leaf)) {
    effects.invocations.push_back(
        Invocation::callback([gesture, place = contender.leaf] {
          if (auto* held = std::get_if<composition::LongPressGesture>(
                  &gesture->layout().leaf(place))) {
            held->handlers().ended(true);
          }
        }));
  } else if (std::holds_alternative<composition::DragGesture>(leaf)) {
    effects.invocations.push_back(Invocation::callback(
        [gesture, place = contender.leaf, ended = value(contender, event)] {
          if (auto* drag = std::get_if<composition::DragGesture>(
                  &gesture->layout().leaf(place))) {
            drag->handlers().ended(ended);
          }
        }));
  } else if (std::holds_alternative<composition::MagnifyGesture>(leaf) &&
             std::holds_alternative<PinchRecognizer>(contender.recognizer)) {
    effects.invocations.push_back(Invocation::callback(
        [gesture, place = contender.leaf, scaled = magnified(contender)] {
          if (auto* magnify = std::get_if<composition::MagnifyGesture>(
                  &gesture->layout().leaf(place))) {
            magnify->handlers().ended(scaled);
          }
        }));
  } else if (std::holds_alternative<composition::RotateGesture>(leaf) &&
             std::holds_alternative<PinchRecognizer>(contender.recognizer)) {
    effects.invocations.push_back(Invocation::callback(
        [gesture, place = contender.leaf, turned = rotated(contender)] {
          if (auto* rotate = std::get_if<composition::RotateGesture>(
                  &gesture->layout().leaf(place))) {
            rotate->handlers().ended(turned);
          }
        }));
  }
}

void GestureArena::claim(composition::Tree& tree, std::size_t index,
                         const Event& event, ScrollCoordinator& scrolls,
                         Effects& effects) {
  Contender& contender = contenders_[index];
  contender.waiting = false;
  contender.fired = true;
  effects.changed = true;
  switch (contender.kind) {
    case Kind::activation:
      effects.focus = contender.node;
      effects.invocations.push_back(Invocation::activation(
          contender.node,
          composition::Point{.x = contender.start.x - contender.origin.x,
                             .y = contender.start.y - contender.origin.y}));
      effects.sounds.push_back(provider::SoundRole::touch);
      if (pressed_ == contender.node) {
        pressed_.reset();
      }
      contender.done = true;
      return;
    case Kind::focus:
      effects.focus = contender.node;
      contender.done = true;
      return;
    case Kind::scroll:
      contender.anchor = event.point;
      scrolls.grab(contender.node, event.now);
      return;
    case Kind::lift:
      effects.lifted = Lift{.node = contender.node, .point = event.point};
      tree.request_feedback(contender.node,
                            composition::SensoryFeedback::impact(
                                composition::SensoryFeedback::Weight::medium),
                            composition::FeedbackOrigin::system);
      return;
    case Kind::gesture:
      break;
  }
  if (gesture_of(tree, contender) == nullptr) {
    contender.done = true;
    return;
  }
  if (std::holds_alternative<DragRecognizer>(contender.recognizer) ||
      std::holds_alternative<PinchRecognizer>(contender.recognizer)) {
    change(tree, index, event, scrolls, effects);
    return;
  }
  contender.done = true;
  if (std::holds_alternative<LongPressRecognizer>(contender.recognizer)) {
    tree.request_feedback(contender.node,
                          composition::SensoryFeedback::impact(
                              composition::SensoryFeedback::Weight::medium),
                          composition::FeedbackOrigin::system);
  }
  notify_ended(tree, index, event, effects);
}

void GestureArena::change(composition::Tree& tree, std::size_t index,
                          const Event& event, ScrollCoordinator& scrolls,
                          Effects& effects) {
  Contender& contender = contenders_[index];
  effects.changed = true;
  switch (contender.kind) {
    case Kind::scroll: {
      scrolls.drag(
          contender.node,
          composition::Size{.width = event.point.x - contender.anchor.x,
                            .height = event.point.y - contender.anchor.y},
          event.now);
      if (overflows(index, event, scrolls)) {
        if (const auto taker = handoff()) {
          transfer(tree, index, *taker, event, scrolls, effects);
        }
      }
      return;
    }
    case Kind::lift:
      effects.dragged = event.point;
      return;
    case Kind::activation:
    case Kind::focus:
      return;
    case Kind::gesture:
      break;
  }
  notify_changed(tree, index, event, effects);
}

void GestureArena::finish(composition::Tree& tree, std::size_t index,
                          const Event& event, ScrollCoordinator& scrolls,
                          Effects& effects) {
  Contender& contender = contenders_[index];
  contender.done = true;
  effects.changed = true;
  switch (contender.kind) {
    case Kind::scroll:
      scrolls.release(contender.node, velocity_.velocity(event.now), event.now);
      return;
    case Kind::lift:
      effects.dropped = event.point;
      return;
    case Kind::activation:
    case Kind::focus:
      return;
    case Kind::gesture:
      break;
  }
  notify_ended(tree, index, event, effects);
}

auto GestureArena::handoff() const -> std::optional<std::size_t> {
  for (std::size_t index = 0; index < contenders_.size(); ++index) {
    const Contender& contender = contenders_[index];
    if (contender.kind == Kind::gesture &&
        groups_[contender.group].handoff.has_value() &&
        std::holds_alternative<DragRecognizer>(contender.recognizer)) {
      return index;
    }
  }
  return std::nullopt;
}

auto GestureArena::declines(std::size_t index,
                            const ScrollCoordinator& scrolls) const -> bool {
  const auto taker = handoff();
  if (!taker.has_value() ||
      contenders_[*taker].depth <= contenders_[index].depth) {
    return false;
  }
  const float rise = last_.y - start_.y;
  const float across = std::abs(last_.x - start_.x);
  if (std::abs(rise) < across) {
    return false;
  }
  const composition::ScrollHandoff handed =
      *groups_[contenders_[*taker].group].handoff;
  if (rise > 0.0F) {
    return scrolls.at_start(contenders_[index].node,
                            composition::Axis::vertical);
  }
  return handed.expands;
}

auto GestureArena::overflows(std::size_t index, const Event& event,
                             const ScrollCoordinator& scrolls) const -> bool {
  const Contender& contender = contenders_[index];
  return event.kind == Event::Kind::move &&
         event.point.y > contender.anchor.y &&
         scrolls.at_start(contender.node, composition::Axis::vertical) &&
         scrolls.overscrolled(contender.node);
}

void GestureArena::transfer(composition::Tree& tree, std::size_t from,
                            std::size_t to, const Event& event,
                            ScrollCoordinator& scrolls, Effects& effects) {
  Contender& scroll = contenders_[from];
  if (contenders_[to].depth <= scroll.depth) {
    return;
  }
  scrolls.release(scroll.node, composition::Size{}, event.now);
  scroll.done = true;
  scroll.lost = true;
  Contender& taker = contenders_[to];
  taker.recognizer = DragRecognizer{0.0F};
  static_cast<void>(signal(taker.recognizer, Event{.kind = Event::Kind::down,
                                                   .point = event.point,
                                                   .now = event.now,
                                                   .second = std::nullopt}));
  taker.start = event.point;
  taker.done = false;
  taker.lost = false;
  taker.waiting = false;
  taker.fired = true;
  winner_ = to;
  velocity_.reset(event.point, event.now);
  notify_changed(tree, to, event, effects);
}

void GestureArena::handle(composition::Tree& tree, std::size_t index,
                          const RecognizerSignals& signals, const Event& event,
                          ScrollCoordinator& scrolls, Effects& effects) {
  Contender& contender = contenders_[index];
  if (signals.pressing.has_value() && contender.kind == Kind::gesture) {
    press(tree, index, *signals.pressing, effects);
  }
  if (signals.failed) {
    lose(tree, index, effects);
    return;
  }
  if (signals.claimed && !contender.fired) {
    if (contender.kind == Kind::scroll && declines(index, scrolls)) {
      lose(tree, index, effects);
      return;
    }
    const bool joined = winner_.has_value() && allied(*winner_, index);
    if (contender.priority == composition::GesturePriority::simultaneous ||
        joined) {
      claim(tree, index, event, scrolls, effects);
    } else {
      contender.waiting = true;
    }
    return;
  }
  if (!contender.fired || contender.done) {
    return;
  }
  if (signals.ended) {
    finish(tree, index, event, scrolls, effects);
  } else if (signals.changed) {
    change(tree, index, event, scrolls, effects);
  }
}

auto GestureArena::unblocked(std::size_t index) const -> bool {
  return std::ranges::all_of(
      contenders_[index].blockers, [this](std::size_t blocker) {
        return contenders_[blocker].done && contenders_[blocker].lost;
      });
}

void GestureArena::resolve(composition::Tree& tree, const Event& event,
                           ScrollCoordinator& scrolls, Effects& effects) {
  if (winner_.has_value()) {
    return;
  }
  for (std::size_t index = 0; index < contenders_.size(); ++index) {
    const Contender& contender = contenders_[index];
    if (!contender.waiting || contender.done || !unblocked(index)) {
      continue;
    }
    winner_ = index;
    for (std::size_t other = 0; other < contenders_.size(); ++other) {
      if (other != index &&
          contenders_[other].priority !=
              composition::GesturePriority::simultaneous &&
          !allied(index, other)) {
        cancel(tree, other, scrolls, effects);
      }
    }
    claim(tree, index, event, scrolls, effects);
    for (std::size_t other = 0; other < contenders_.size(); ++other) {
      if (other != index && contenders_[other].waiting &&
          !contenders_[other].done && allied(index, other)) {
        claim(tree, other, event, scrolls, effects);
      }
    }
    return;
  }
}

void GestureArena::arm(composition::Tree& tree, const Event& event,
                       ScrollCoordinator& scrolls, Effects& effects) {
  for (std::size_t index = 0; index < contenders_.size(); ++index) {
    Contender& contender = contenders_[index];
    if (contender.done || contender.armed) {
      continue;
    }
    if (stranded(index)) {
      lose(tree, index, effects);
      continue;
    }
    if (gated(index)) {
      continue;
    }
    contender.armed = true;
    if (touching_) {
      contender.start = last_;
      const Event begun{.kind = Event::Kind::down,
                        .point = last_,
                        .now = event.now,
                        .second = second_};
      handle(tree, index, signal(contender.recognizer, begun), begun, scrolls,
             effects);
    } else {
      contender.awaiting = true;
      contender.deadline = event.now + sequence_window;
    }
  }
}

void GestureArena::clear() {
  contenders_.clear();
  groups_.clear();
  winner_.reset();
  pressed_.reset();
}

void GestureArena::tidy() {
  if (touching_) {
    return;
  }
  const bool calm =
      std::ranges::all_of(contenders_, [](const Contender& contender) {
        return contender.done ||
               (!contender.waiting && !contender.awaiting &&
                (!contender.armed || !possible(contender.recognizer)));
      });
  if (calm) {
    clear();
  }
}

void GestureArena::feed(composition::Tree& tree, const Event& event,
                        ScrollCoordinator& scrolls, Effects& effects) {
  last_now_ = event.now;
  for (std::size_t index = 0; index < contenders_.size(); ++index) {
    Contender& contender = contenders_[index];
    if (contender.done || !contender.armed) {
      continue;
    }
    if (winner_.has_value() && *winner_ != index &&
        contender.priority != composition::GesturePriority::simultaneous &&
        !allied(*winner_, index)) {
      continue;
    }
    if (contender.awaiting) {
      if (event.kind == Event::Kind::down) {
        contender.awaiting = false;
        contender.start = event.point;
      } else {
        if (event.now > contender.deadline) {
          lose(tree, index, effects);
        }
        continue;
      }
    }
    const RecognizerSignals signals = signal(contender.recognizer, event);
    handle(tree, index, signals, event, scrolls, effects);
  }
  resolve(tree, event, scrolls, effects);
  arm(tree, event, scrolls, effects);
  tidy();
}

void GestureArena::down(composition::Tree& tree, composition::Point point,
                        std::chrono::nanoseconds now,
                        ScrollCoordinator& scrolls,
                        std::optional<composition::NodeId> confined,
                        Effects& effects) {
  down(tree, point, now, scrolls, confined,
       composition::GestureInputKinds::direct_touch, effects);
}

void GestureArena::down(composition::Tree& tree, composition::Point point,
                        std::chrono::nanoseconds now,
                        ScrollCoordinator& scrolls,
                        std::optional<composition::NodeId> confined,
                        composition::GestureInputKinds kind, Effects& effects) {
  const Event event{.kind = Event::Kind::down,
                    .point = point,
                    .now = now,
                    .second = std::nullopt};
  second_.reset();
  if (active()) {
    const bool joins =
        std::hypot(point.x - start_.x, point.y - start_.y) <=
            TapRecognizer::default_spread &&
        std::ranges::any_of(contenders_, [](const Contender& contender) {
          return !contender.done &&
                 (contender.awaiting || waiting(contender.recognizer));
        });
    if (joins) {
      touching_ = true;
      last_ = point;
      velocity_.reset(point, now);
      feed(tree, event, scrolls, effects);
      return;
    }
    for (std::size_t index = 0; index < contenders_.size(); ++index) {
      if (!contenders_[index].done && !contenders_[index].waiting) {
        lose(tree, index, effects);
      }
    }
    resolve(tree, event, scrolls, effects);
    for (std::size_t index = 0; index < contenders_.size(); ++index) {
      cancel(tree, index, scrolls, effects);
    }
    clear();
  }
  kind_ = kind;
  build(tree, point, scrolls, confined);
  touching_ = true;
  start_ = point;
  last_ = point;
  velocity_.reset(point, now);
  effects.changed = true;
  feed(tree, event, scrolls, effects);
}

void GestureArena::move(composition::Tree& tree, composition::Point point,
                        std::chrono::nanoseconds now,
                        ScrollCoordinator& scrolls, Effects& effects) {
  if (!active() || !touching_) {
    return;
  }
  last_ = point;
  velocity_.add(point, now);
  feed(tree,
       Event{.kind = Event::Kind::move,
             .point = point,
             .now = now,
             .second = second_},
       scrolls, effects);
}

void GestureArena::second(composition::Tree& tree,
                          std::optional<composition::Point> point,
                          std::chrono::nanoseconds now,
                          ScrollCoordinator& scrolls, Effects& effects) {
  const bool arriving = point.has_value() && !second_.has_value();
  second_ = point;
  if (!active() || !touching_) {
    return;
  }
  if (arriving) {
    const bool pinching =
        std::ranges::any_of(contenders_, [](const Contender& contender) {
          return !contender.done && contender.armed &&
                 std::holds_alternative<PinchRecognizer>(contender.recognizer);
        });
    for (std::size_t index = 0; index < contenders_.size(); ++index) {
      Contender& contender = contenders_[index];
      if (contender.done || contender.fired) {
        continue;
      }
      const bool single = contender.kind == Kind::activation ||
                          contender.kind == Kind::lift ||
                          contender.kind == Kind::focus ||
                          (contender.kind == Kind::gesture &&
                           single_finger(contender.recognizer));
      if (single || (contender.kind == Kind::scroll && pinching)) {
        std::visit([](auto& active) { static_cast<void>(active.cancel()); },
                   contender.recognizer);
        lose(tree, index, effects);
      }
    }
  }
  effects.changed = true;
  feed(tree,
       Event{.kind = Event::Kind::move,
             .point = last_,
             .now = now,
             .second = second_},
       scrolls, effects);
}

void GestureArena::up(composition::Tree& tree, composition::Point point,
                      std::chrono::nanoseconds now, ScrollCoordinator& scrolls,
                      Effects& effects) {
  if (!active() || !touching_) {
    touching_ = false;
    return;
  }
  touching_ = false;
  last_ = point;
  second_.reset();
  velocity_.add(point, now);
  effects.changed = true;
  feed(tree,
       Event{.kind = Event::Kind::up,
             .point = point,
             .now = now,
             .second = std::nullopt},
       scrolls, effects);
}

void GestureArena::abandon(composition::Tree& tree, ScrollCoordinator& scrolls,
                           Effects& effects) {
  for (std::size_t index = 0; index < contenders_.size(); ++index) {
    cancel(tree, index, scrolls, effects);
  }
  touching_ = false;
  second_.reset();
  tidy();
}

void GestureArena::elapse(composition::Tree& tree, std::chrono::nanoseconds now,
                          ScrollCoordinator& scrolls, Effects& effects) {
  if (!active()) {
    return;
  }
  feed(tree,
       Event{.kind = Event::Kind::elapse,
             .point = last_,
             .now = now,
             .second = second_},
       scrolls, effects);
}

}
