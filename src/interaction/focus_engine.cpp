#include <interaction/focus_engine.hpp>

#include <arisa/composition/layer.hpp>
#include <arisa/composition/node_lifecycle_phase.hpp>
#include <arisa/composition/paint_order.hpp>

#include <algorithm>
#include <functional>
#include <limits>
#include <ranges>

namespace arisa::interaction {

void FocusEngine::collect(const composition::Tree& tree) {
  entries_.clear();
  sections_.clear();
  spans_.clear();
  traps_.clear();
  bound_.clear();
  defaults_.clear();
  path_.clear();
  paths_.clear();
  scopes_.clear();
  scrollers_.clear();
  chains_.clear();
  stale_ = false;
  order_ = 0;
  const composition::Node& root = tree.root();
  root_ = root.id();
  const composition::ExclusiveScopes& exclusive = tree.exclusive();
  if (exclusive.active()) {
    kept_ = exclusive.kept() |
            std::views::transform(&composition::OverlayLayer::z) |
            std::ranges::to<std::vector>();
    exclusive_owner_ = exclusive.owner().value_or(root_);
  } else {
    kept_.reset();
    exclusive_owner_.reset();
  }
  sections_.insert_or_assign(
      root_, Section{.parent = std::nullopt, .frame = root.frame()});
  Visitor visitor{*this};
  traversal_.run(
      root,
      Visit{.walk = Walk{.section = root_,
                         .layer = composition::OverlayLayer::content.z(),
                         .disabled = false,
                         .hidden = false,
                         .key = nullptr,
                         .path_base = 0,
                         .scrolled = {}}},
      visitor);
  std::ranges::sort(spans_, {}, &Spanned::id);
}

auto FocusEngine::chain() -> Span {
  const std::size_t begin = chains_.size();
  chains_.insert(chains_.end(), scrollers_.begin(), scrollers_.end());
  return Span{.begin = begin, .end = chains_.size()};
}

auto FocusEngine::shown(const composition::Tree& tree, composition::NodeId id,
                        Span chain) const -> std::optional<composition::Rect> {
  const auto node = tree.node(id);
  if (!node.has_value()) {
    return std::nullopt;
  }
  if (const auto& presented = node->get().presented(); presented.has_value()) {
    return presented->bounds;
  }
  composition::Rect frame = node->get().frame();
  for (const composition::NodeId scroller :
       std::span{chains_}.subspan(chain.begin, chain.end - chain.begin)) {
    if (const auto held = tree.node(scroller)) {
      frame.x -= held->get().scroll_presentation().offset.x;
      frame.y -= held->get().scroll_presentation().offset.y;
    }
  }
  return frame;
}

void FocusEngine::refresh(const composition::Tree& tree) {
  if (!stale_) {
    return;
  }
  stale_ = false;
  ++refreshes_;
  for (Entry& entry : entries_) {
    entry.frame = shown(tree, entry.id, entry.chain).value_or(entry.frame);
  }
  for (auto&& [id, section] : sections_) {
    section.frame = shown(tree, id, section.chain).value_or(section.frame);
  }
}

auto FocusEngine::Visitor::enter(const composition::Node& node,
                                 const Visit& above) -> Traversal::Step {
  return engine_.get().visit(node, above.walk);
}

auto FocusEngine::Visitor::next(const composition::Node& node, Visit& visit)
    -> const composition::Node* {
  FocusEngine& engine = engine_;
  if (visit.pathed) {
    engine.path_.pop_back();
    visit.pathed = false;
  }
  const auto children = node.children();
  if (visit.index >= std::ranges::size(children)) {
    return nullptr;
  }
  engine.path_.push_back(visit.index);
  visit.pathed = true;
  return &children[visit.index++];
}

void FocusEngine::Visitor::leave(const composition::Node& node, Visit& visit) {
  engine_.get().leave(node, visit);
}

auto FocusEngine::visit(const composition::Node& node, Walk walk)
    -> Traversal::Step {
  const bool top = !node.parent().has_value();
  if (!top && node.phase() != composition::NodeLifecyclePhase::live) {
    return Traversal::Step{.state = Visit{}, .descend = false};
  }
  if (const auto& accessible = node.interactions().accessibility;
      accessible != nullptr && accessible->layer_content) {
    return Traversal::Step{.state = Visit{}, .descend = false};
  }
  const auto layer = node.content_as<composition::Layer::Content>();
  if (layer.has_value()) {
    if (layer->get().input() == composition::OverlayInput::passthrough ||
        !reachable(layer->get().layer().z())) {
      return Traversal::Step{.state = Visit{}, .descend = false};
    }
    walk.layer = layer->get().layer().z();
  }
  const bool reached = reachable(walk.layer);
  const std::size_t begin = order_++;
  const std::size_t scope_depth = scopes_.size();
  horizon_ = std::max(horizon_, node.id());
  const std::optional<composition::PresentedFrame>& presented =
      node.presented();
  walk.hidden = walk.hidden || (presented.has_value() && !presented->visible);
  const composition::Interactions& declared = node.interactions();
  walk.disabled = walk.disabled || declared.disabled;
  const composition::Rect laid = node.frame();
  const composition::Rect frame =
      presented.has_value() ? presented->bounds
                            : composition::Rect{.x = laid.x - walk.scrolled.x,
                                                .y = laid.y - walk.scrolled.y,
                                                .width = laid.width,
                                                .height = laid.height};
  if (node.key().has_value()) {
    walk.key = &*node.key();
    walk.path_base = path_.size();
  }
  if (!declared.focused.empty()) {
    bound_.push_back(node.id());
  }
  for (std::size_t index = 0; index < declared.default_focus.size(); ++index) {
    defaults_.push_back(Declaration{.declarer = node.id(), .index = index});
  }
  if (declared.focus_scope.has_value()) {
    scopes_.push_back(declared.focus_scope->id);
  }
  walk.prefers =
      walk.prefers ||
      (declared.prefers_default_focus.has_value() &&
       std::ranges::contains(scopes_, *declared.prefers_default_focus));
  const bool focusable =
      declared.focusable.value_or(node.content().activatable());
  std::optional<std::size_t> fallback;
  if (!top && reached && focusable && !walk.disabled && !walk.hidden) {
    if (declared.focus_fallback) {
      fallback = entries_.size();
    }
    const bool prefers = walk.prefers;
    const std::size_t path_begin = paths_.size();
    paths_.insert(paths_.end(),
                  path_.begin() + static_cast<std::ptrdiff_t>(walk.path_base),
                  path_.end());
    const std::optional<composition::MatchedGeometry>& geometry =
        node.matched_geometry();
    entries_.push_back(
        Entry{.id = node.id(),
              .section = walk.section,
              .frame = frame,
              .prefers = prefers,
              .key = walk.key,
              .path_begin = path_begin,
              .path_size = paths_.size() - path_begin,
              .type = node.type(),
              .matched = geometry.has_value() ? &geometry->key : nullptr,
              .chain = chain()});
  }
  const bool modal = layer.has_value() &&
                     layer->get().input() == composition::OverlayInput::modal;
  const bool confines = ((declared.focus_scope.has_value() &&
                          declared.focus_scope->behavior ==
                              composition::FocusScopeBehavior::confined) ||
                         modal) &&
                        !walk.hidden;
  if (!top && reached && (declared.focus_section || confines)) {
    sections_.insert_or_assign(
        node.id(),
        Section{.parent = walk.section, .frame = frame, .chain = chain()});
    walk.section = node.id();
    if (confines) {
      traps_.push_back(Trap{.id = node.id(), .z = walk.layer});
    }
  }
  const bool scrolls = declared.scrolling.container.has_value();
  if (scrolls) {
    walk.scrolled.x += node.scroll_presentation().offset.x;
    walk.scrolled.y += node.scroll_presentation().offset.y;
    scrollers_.push_back(node.id());
  }
  return Traversal::Step{.state = Visit{.walk = walk,
                                        .begin = begin,
                                        .scope_depth = scope_depth,
                                        .fallback = fallback,
                                        .index = 0,
                                        .entered = true,
                                        .scrolls = scrolls,
                                        .pathed = false}};
}

void FocusEngine::leave(const composition::Node& node, Visit& visit) {
  if (!visit.entered) {
    return;
  }
  if (visit.scrolls) {
    scrollers_.pop_back();
  }
  scopes_.erase(
      scopes_.begin() + static_cast<std::ptrdiff_t>(visit.scope_depth),
      scopes_.end());
  if (visit.fallback.has_value() && entries_.size() > *visit.fallback + 1) {
    entries_.erase(entries_.begin() +
                   static_cast<std::ptrdiff_t>(*visit.fallback));
  }
  spans_.push_back(Spanned{.id = node.id(),
                           .span = Span{.begin = visit.begin, .end = order_}});
}

auto FocusEngine::path_of(const Entry& entry) const
    -> std::span<const std::size_t> {
  return std::span<const std::size_t>{paths_}.subspan(entry.path_begin,
                                                      entry.path_size);
}

auto FocusEngine::span_of(composition::NodeId node) const
    -> std::optional<Span> {
  const auto found = std::ranges::lower_bound(spans_, node, {}, &Spanned::id);
  if (found == spans_.end() || found->id != node) {
    return std::nullopt;
  }
  return found->span;
}

auto FocusEngine::index_of(composition::NodeId id) const
    -> std::optional<std::size_t> {
  const auto found = std::ranges::find(entries_, id, &Entry::id);
  if (found == entries_.end()) {
    return std::nullopt;
  }
  return static_cast<std::size_t>(found - entries_.begin());
}

auto FocusEngine::inside(composition::NodeId node,
                         composition::NodeId container) const -> bool {
  const std::optional<Span> inner = span_of(node);
  const std::optional<Span> outer = span_of(container);
  if (!inner.has_value() || !outer.has_value()) {
    return false;
  }
  return outer->begin <= inner->begin && inner->begin < outer->end;
}

auto FocusEngine::confined_to() const noexcept
    -> std::optional<composition::NodeId> {
  std::optional<Trap> chosen;
  for (const Trap& trap : traps_) {
    if (!chosen.has_value() || trap.z >= chosen->z) {
      chosen = trap;
    }
  }
  return chosen.transform([](const Trap& trap) { return trap.id; });
}

auto FocusEngine::reachable(std::int32_t layer) const noexcept -> bool {
  return !kept_.has_value() || std::ranges::contains(*kept_, layer);
}

void FocusEngine::restore(composition::Tree& tree) {
  const std::optional<composition::NodeId> restored =
      tree.exclusive().take_restore();
  if (restored.has_value() && valid(*restored)) {
    focused_ = restored;
    dormant_ = false;
    pending_restore_.reset();
  }
}

auto FocusEngine::trapped(composition::NodeId id) const -> bool {
  return std::ranges::contains(traps_, id, &Trap::id);
}

auto FocusEngine::boundary() const noexcept -> composition::NodeId {
  return confined_to().value_or(root_);
}

auto FocusEngine::eligible(const Entry& entry) const -> bool {
  const std::optional<composition::NodeId> trap = confined_to();
  return !trap.has_value() || inside(entry.id, *trap);
}

auto FocusEngine::valid(composition::NodeId id) const -> bool {
  const auto index = index_of(id);
  return index.has_value() && eligible(entries_[*index]);
}

auto FocusEngine::origin() const noexcept
    -> std::optional<composition::NodeId> {
  if (focused_.has_value()) {
    return focused_;
  }
  if (const auto trap = confined_to()) {
    return trap;
  }
  return exclusive_owner_;
}

auto FocusEngine::populated(composition::NodeId section,
                            std::optional<composition::NodeId> excluded) const
    -> bool {
  return std::ranges::any_of(entries_, [&](const Entry& entry) {
    return entry.id != excluded && eligible(entry) && inside(entry.id, section);
  });
}

auto FocusEngine::units(composition::NodeId level,
                        std::optional<composition::NodeId> skipped,
                        std::optional<composition::NodeId> excluded) const
    -> std::vector<Unit> {
  std::vector<Unit> found;
  for (const Entry& entry : entries_) {
    if (entry.section == level && entry.id != excluded && eligible(entry)) {
      found.push_back(
          Unit{.id = entry.id, .section = false, .frame = entry.frame});
    }
  }
  for (const auto& [id, section] : sections_) {
    if (section.parent == level && id != skipped && populated(id, excluded)) {
      found.push_back(Unit{.id = id, .section = true, .frame = section.frame});
    }
  }
  return found;
}

auto FocusEngine::remembered(composition::NodeId section) const
    -> std::optional<composition::NodeId> {
  const auto found = memory_.find(section);
  if (found == memory_.end() || !valid(found->second) ||
      !inside(found->second, section)) {
    return std::nullopt;
  }
  return found->second;
}

auto FocusEngine::type_select_label(const composition::Node& node)
    -> std::optional<std::string> {
  const composition::Interactions& declared = node.interactions();
  if (declared.type_select_cleared) {
    return std::nullopt;
  }
  if (declared.type_select.has_value()) {
    return declared.type_select;
  }
  if (declared.accessibility != nullptr &&
      declared.accessibility->label.has_value()) {
    return declared.accessibility->label->verbatim();
  }
  return node.content().accessibility().label;
}

auto FocusEngine::type_select(const composition::Tree& tree,
                              std::string_view prefix) const
    -> std::optional<composition::NodeId> {
  if (prefix.empty()) {
    return std::nullopt;
  }
  const auto lowered = [](char each) {
    return each >= 'A' && each <= 'Z' ? static_cast<char>(each - 'A' + 'a')
                                      : each;
  };
  const auto matches = [&](const Entry& entry) {
    const auto node = tree.node(entry.id);
    if (!node.has_value()) {
      return false;
    }
    const std::optional<std::string> label = type_select_label(node->get());
    return label.has_value() && label->size() >= prefix.size() &&
           std::ranges::equal(prefix,
                              std::string_view{*label}.substr(0, prefix.size()),
                              {}, lowered, lowered);
  };
  std::optional<composition::NodeId> section;
  std::size_t start = 0;
  if (focused_.has_value()) {
    const auto current = std::ranges::find(entries_, *focused_, &Entry::id);
    if (current != entries_.end()) {
      section = current->section;
      start = static_cast<std::size_t>(current - entries_.begin());
      const bool repeated = prefix.size() == 1 ||
                            std::ranges::all_of(prefix, [&prefix](char each) {
                              return each == prefix.front();
                            });
      if (repeated && matches(*current)) {
        start += 1;
      }
    }
  }
  const bool engaged = std::ranges::any_of(entries_, [&](const Entry& entry) {
    if (section.has_value() && entry.section != *section) {
      return false;
    }
    const auto node = tree.node(entry.id);
    return node.has_value() &&
           node->get().interactions().type_select.has_value();
  });
  if (!engaged) {
    return std::nullopt;
  }
  for (std::size_t offset = 0; offset < entries_.size(); ++offset) {
    const Entry& entry = entries_[(start + offset) % entries_.size()];
    if ((!section.has_value() || entry.section == *section) &&
        eligible(entry) && matches(entry)) {
      return entry.id;
    }
  }
  return std::nullopt;
}

auto FocusEngine::declared(const composition::Tree& tree,
                           composition::NodeId section, bool own) const
    -> std::optional<composition::NodeId> {
  return declared(tree, section, own, std::nullopt);
}

auto FocusEngine::declared(
    const composition::Tree& tree, composition::NodeId section, bool own,
    std::optional<composition::DefaultFocusEvaluationPriority> only) const
    -> std::optional<composition::NodeId> {
  for (const Declaration& declaration : defaults_) {
    if (own && declaration.declarer != section) {
      continue;
    }
    if (!inside(declaration.declarer, section) &&
        !inside(section, declaration.declarer)) {
      continue;
    }
    const auto declarer = tree.node(declaration.declarer);
    if (!declarer.has_value()) {
      continue;
    }
    const composition::Interactions::DefaultFocus& declared_default =
        declarer->get().interactions().default_focus[declaration.index];
    if (only.has_value() && declared_default.priority != *only) {
      continue;
    }
    const composition::FocusRequest& wanted = *declared_default.request;
    for (const Entry& entry : entries_) {
      if (!eligible(entry) || !inside(entry.id, section) ||
          !inside(entry.id, declaration.declarer)) {
        continue;
      }
      const auto node = tree.node(entry.id);
      if (node.has_value() &&
          std::ranges::any_of(
              node->get().interactions().focused,
              [&wanted](
                  const std::unique_ptr<composition::FocusRequest>& request) {
                return request->same_target(wanted);
              })) {
        return entry.id;
      }
    }
  }
  return std::nullopt;
}

auto FocusEngine::preferred(const composition::Tree& tree,
                            composition::NodeId section) const
    -> std::optional<composition::NodeId> {
  if (const auto chosen = declared(tree, section, false)) {
    return chosen;
  }
  for (const Entry& entry : entries_) {
    if (entry.prefers && eligible(entry) && inside(entry.id, section)) {
      return entry.id;
    }
  }
  return std::nullopt;
}

auto FocusEngine::enter(const composition::Tree& tree,
                        composition::NodeId section,
                        const composition::Rect& from,
                        composition::MoveCommandDirection direction) const
    -> std::optional<composition::NodeId> {
  if (const auto own = declared(
          tree, section, true,
          composition::DefaultFocusEvaluationPriority::user_initiated)) {
    return own;
  }
  if (const auto memory = remembered(section)) {
    return memory;
  }
  if (const auto own = declared(tree, section, true)) {
    return own;
  }
  if (const auto chosen = preferred(tree, section)) {
    return chosen;
  }
  const std::vector<Unit> inner = units(section, std::nullopt, focused_);
  std::vector<composition::Rect> frames;
  frames.reserve(inner.size());
  for (const Unit& unit : inner) {
    frames.push_back(unit.frame);
  }
  std::optional<std::size_t> pick = search_.nearest(from, direction, frames);
  if (!pick.has_value()) {
    pick = SpatialSearch::closest(from, frames);
  }
  if (!pick.has_value()) {
    return std::nullopt;
  }
  const Unit& unit = inner[*pick];
  if (unit.section) {
    return enter(tree, unit.id, from, direction);
  }
  return unit.id;
}

auto FocusEngine::nearest_inside(composition::NodeId container,
                                 std::optional<composition::NodeId> section,
                                 const composition::Rect& from) const
    -> std::optional<composition::NodeId> {
  std::vector<composition::NodeId> ids;
  std::vector<composition::Rect> frames;
  for (const Entry& entry : entries_) {
    if (eligible(entry) && inside(entry.id, container) &&
        (!section.has_value() || inside(entry.id, *section))) {
      ids.push_back(entry.id);
      frames.push_back(entry.frame);
    }
  }
  const auto pick = SpatialSearch::closest(from, frames);
  if (!pick.has_value()) {
    return std::nullopt;
  }
  return ids[*pick];
}

auto FocusEngine::visible_inside(const composition::Tree& tree,
                                 composition::NodeId container,
                                 const composition::Rect& viewport)
    -> std::optional<composition::NodeId> {
  if (!focused_.has_value() || !inside(*focused_, container)) {
    return std::nullopt;
  }
  refresh(tree);
  const auto within = [&viewport](const composition::Rect& frame) {
    return frame.y >= viewport.y &&
           frame.y + frame.height <= viewport.y + viewport.height;
  };
  const auto current = index_of(*focused_);
  if (!current.has_value()) {
    return std::nullopt;
  }
  if (within(entries_[*current].frame)) {
    return std::nullopt;
  }
  const float middle = viewport.y + (viewport.height * 0.5F);
  std::optional<composition::NodeId> chosen;
  float best = std::numeric_limits<float>::max();
  for (const Entry& entry : entries_) {
    if (!eligible(entry) || entry.id == *focused_ ||
        !inside(entry.id, container) || !within(entry.frame)) {
      continue;
    }
    const float distance =
        std::abs(entry.frame.y + (entry.frame.height * 0.5F) - middle);
    if (distance < best) {
      best = distance;
      chosen = entry.id;
    }
  }
  return chosen;
}

auto FocusEngine::first_inside(composition::NodeId container) const
    -> std::optional<composition::NodeId> {
  const auto found = std::ranges::find_if(entries_, [&](const Entry& entry) {
    return eligible(entry) && inside(entry.id, container);
  });
  if (found == entries_.end()) {
    return std::nullopt;
  }
  return found->id;
}

auto FocusEngine::advance(const composition::Tree& tree,
                          composition::MoveCommandDirection direction) const
    -> std::optional<composition::NodeId> {
  const auto index = focused_.has_value() ? index_of(*focused_) : std::nullopt;
  if (!index.has_value()) {
    return std::nullopt;
  }
  const Entry& current = entries_[*index];
  const composition::Rect from = aimed(current, direction);
  composition::NodeId level = current.section;
  std::optional<composition::NodeId> skipped;
  const composition::NodeId limit = boundary();
  while (true) {
    const std::vector<Unit> found = units(level, skipped, current.id);
    std::vector<composition::Rect> frames;
    std::vector<std::size_t> kept;
    frames.reserve(found.size());
    kept.reserve(found.size());
    for (std::size_t at = 0; at < found.size(); ++at) {
      const Unit& unit = found[at];
      const bool beneath =
          unit.section && underlies(unit.frame, current.frame) &&
          !SpatialSearch::qualifies(current.frame, direction, unit.frame);
      if (beneath && !leads(unit.id, current, direction)) {
        continue;
      }
      frames.push_back(beneath ? ahead(unit.frame, current.frame, direction)
                               : unit.frame);
      kept.push_back(at);
    }
    std::optional<std::size_t> pick = search_.nearest(from, direction, frames);
    if (from != current.frame) {
      const std::optional<std::size_t> plain =
          search_.nearest(current.frame, direction, frames);
      if (plain.has_value() &&
          (!pick.has_value() ||
           SpatialSearch::gap(current.frame, direction, frames[*plain]) +
                   SpatialSearch::edge_tolerance <
               SpatialSearch::gap(current.frame, direction, frames[*pick]))) {
        pick = plain;
      }
    }
    if (pick.has_value()) {
      const Unit& unit = found[kept[*pick]];
      if (unit.section) {
        return enter(tree, unit.id, from, direction);
      }
      return unit.id;
    }
    const auto section = sections_.find(level);
    if (level == limit || section == sections_.end() ||
        !section->second.parent.has_value()) {
      return std::nullopt;
    }
    skipped = level;
    level = *section->second.parent;
  }
}

auto FocusEngine::along(composition::MoveCommandDirection direction) noexcept
    -> bool {
  return direction == composition::MoveCommandDirection::up ||
         direction == composition::MoveCommandDirection::down;
}

auto FocusEngine::aimed(const Entry& current,
                        composition::MoveCommandDirection direction) const
    -> composition::Rect {
  composition::Rect frame = current.frame;
  if (!goal_.has_value() || goal_->node != current.id ||
      goal_->vertical != along(direction)) {
    return frame;
  }
  if (goal_->vertical) {
    frame.x = goal_->center - (frame.width * 0.5F);
  } else {
    frame.y = goal_->center - (frame.height * 0.5F);
  }
  return frame;
}

void FocusEngine::aim(composition::NodeId from, composition::NodeId to,
                      composition::MoveCommandDirection direction) {
  const bool vertical = along(direction);
  if (goal_.has_value() && goal_->node == from && goal_->vertical == vertical) {
    goal_->node = to;
    return;
  }
  const auto index = index_of(from);
  if (!index.has_value()) {
    goal_.reset();
    return;
  }
  const composition::Rect& frame = entries_[*index].frame;
  goal_ = Goal{.node = to,
               .vertical = vertical,
               .center = vertical ? frame.x + (frame.width * 0.5F)
                                  : frame.y + (frame.height * 0.5F)};
}

auto FocusEngine::leads(composition::NodeId section, const Entry& current,
                        composition::MoveCommandDirection direction) const
    -> bool {
  return std::ranges::any_of(entries_, [&](const Entry& entry) {
    return entry.id != current.id && eligible(entry) &&
           inside(entry.id, section) &&
           SpatialSearch::qualifies(current.frame, direction, entry.frame);
  });
}

auto FocusEngine::underlies(const composition::Rect& section,
                            const composition::Rect& from) noexcept -> bool {
  return section.x <= from.x &&
         section.x + section.width >= from.x + from.width &&
         section.y <= from.y &&
         section.y + section.height >= from.y + from.height;
}

auto FocusEngine::ahead(const composition::Rect& section,
                        const composition::Rect& from,
                        composition::MoveCommandDirection direction) noexcept
    -> composition::Rect {
  const float right = section.x + section.width;
  const float bottom = section.y + section.height;
  composition::Rect clipped = section;
  switch (direction) {
    case composition::MoveCommandDirection::right:
      clipped.x = std::max(section.x, from.x + from.width);
      clipped.width = right - clipped.x;
      break;
    case composition::MoveCommandDirection::left:
      clipped.width = std::min(right, from.x) - section.x;
      break;
    case composition::MoveCommandDirection::down:
      clipped.y = std::max(section.y, from.y + from.height);
      clipped.height = bottom - clipped.y;
      break;
    case composition::MoveCommandDirection::up:
      clipped.height = std::min(bottom, from.y) - section.y;
      break;
  }
  return clipped.width > 0.0F && clipped.height > 0.0F ? clipped : section;
}

auto FocusEngine::by_key(const Record& record) const
    -> std::optional<composition::NodeId> {
  if (!record.key.has_value()) {
    return std::nullopt;
  }
  for (const Entry& entry : entries_) {
    if (entry.id > record.horizon && eligible(entry) && entry.key != nullptr &&
        *entry.key == *record.key &&
        std::ranges::equal(path_of(entry), record.path) &&
        entry.type == record.type) {
      return entry.id;
    }
  }
  return std::nullopt;
}

auto FocusEngine::by_geometry(const Record& record) const
    -> std::optional<composition::NodeId> {
  if (!record.matched.has_value()) {
    return std::nullopt;
  }
  const auto found = std::ranges::find_if(entries_, [&](const Entry& entry) {
    return entry.id > record.horizon && eligible(entry) &&
           entry.matched != nullptr && *entry.matched == *record.matched;
  });
  if (found == entries_.end()) {
    return std::nullopt;
  }
  return found->id;
}

auto FocusEngine::overlap(const composition::Rect& left,
                          const composition::Rect& right) noexcept -> float {
  const float width = std::min(left.x + left.width, right.x + right.width) -
                      std::max(left.x, right.x);
  const float height = std::min(left.y + left.height, right.y + right.height) -
                       std::max(left.y, right.y);
  return width > 0.0F && height > 0.0F ? width * height : 0.0F;
}

auto FocusEngine::replacement(const Record& record) const
    -> std::optional<composition::NodeId> {
  std::optional<composition::NodeId> chosen;
  float best = 0.0F;
  for (const Entry& entry : entries_) {
    if (entry.id <= record.horizon || !eligible(entry)) {
      continue;
    }
    const float shared = overlap(entry.frame, record.frame);
    if (shared > best) {
      best = shared;
      chosen = entry.id;
    }
  }
  return chosen;
}

auto FocusEngine::recover(const composition::Tree& tree)
    -> std::optional<composition::NodeId> {
  if (!record_.has_value()) {
    return preferred(tree, boundary()).or_else([this] {
      return first_inside(boundary());
    });
  }
  const Record record = *record_;
  if (const auto restored = by_key(record)) {
    return restored;
  }
  if (const auto hero = by_geometry(record)) {
    return hero;
  }
  if (const auto successor = replacement(record)) {
    return successor;
  }
  if (record.key.has_value()) {
    pending_restore_ = record;
  }
  const std::optional<composition::NodeId> section =
      record.sections.empty()
          ? std::nullopt
          : std::optional<composition::NodeId>{record.sections.front()};
  const bool section_alive =
      section.has_value() && sections_.contains(*section);
  if (record.parent.has_value() && span_of(*record.parent).has_value()) {
    if (const auto sibling = nearest_inside(
            *record.parent,
            section_alive ? section : std::optional<composition::NodeId>{},
            record.frame)) {
      return sibling;
    }
  }
  if (section_alive) {
    if (const auto chosen = preferred(tree, *section)) {
      return chosen;
    }
    if (const auto first = first_inside(*section)) {
      return first;
    }
  }
  for (const composition::NodeId parent :
       record.sections | std::views::drop(1)) {
    if (!sections_.contains(parent)) {
      continue;
    }
    if (const auto near = nearest_inside(parent, std::nullopt, record.frame)) {
      return near;
    }
  }
  return nearest_inside(boundary(), std::nullopt, record.frame);
}

void FocusEngine::confine(const composition::Tree& tree) {
  while (!restores_.empty() && !trapped(restores_.back().trap)) {
    const std::optional<composition::NodeId> saved = restores_.back().saved;
    restores_.pop_back();
    if (saved.has_value() && valid(*saved)) {
      focused_ = saved;
      dormant_ = false;
    }
  }
  const std::optional<composition::NodeId> trap = confined_to();
  if (!trap.has_value() ||
      (!restores_.empty() && restores_.back().trap == *trap)) {
    return;
  }
  restores_.push_back(Restore{.trap = *trap, .saved = focused_});
  if (!focused_.has_value() || !inside(*focused_, *trap)) {
    focused_ = remembered(*trap)
                   .or_else([&] { return preferred(tree, *trap); })
                   .or_else([&] { return first_inside(*trap); });
    dormant_ = false;
  }
}

void FocusEngine::requests(const composition::Tree& tree) {
  std::optional<composition::NodeId> wanted;
  for (const Entry& entry : entries_) {
    if (!eligible(entry) || entry.id == focused_) {
      continue;
    }
    const auto node = tree.node(entry.id);
    if (!node.has_value()) {
      continue;
    }
    const auto& bindings = node->get().interactions().focused;
    for (std::size_t index = 0; index < bindings.size(); ++index) {
      if (bindings[index]->requested() &&
          !requested_.contains({entry.id, index})) {
        wanted = entry.id;
      }
    }
  }
  if (wanted.has_value()) {
    focused_ = wanted;
    dormant_ = false;
    pending_restore_.reset();
    return;
  }
  if (!focused_.has_value()) {
    return;
  }
  const auto node = tree.node(*focused_);
  if (!node.has_value()) {
    return;
  }
  const auto& bindings = node->get().interactions().focused;
  bool held = false;
  bool still = false;
  for (std::size_t index = 0; index < bindings.size(); ++index) {
    if (requested_.contains({*focused_, index})) {
      held = true;
      still = still || bindings[index]->requested();
    }
  }
  if (held && !still) {
    focused_.reset();
    dormant_ = true;
  }
}

void FocusEngine::adopt(composition::Tree& tree,
                        std::optional<composition::NodeId> node) {
  focused_ = node;
  if (focused_.has_value()) {
    if (const auto index = index_of(*focused_)) {
      const Entry& entry = entries_[*index];
      if (!record_.has_value()) {
        record_.emplace(Record{.node = entry.id,
                               .parent = std::nullopt,
                               .sections = {},
                               .frame = entry.frame,
                               .key = std::nullopt,
                               .path = {},
                               .type = entry.type,
                               .matched = std::nullopt,
                               .horizon = horizon_});
      }
      Record& record = *record_;
      record.sections.clear();
      std::optional<composition::NodeId> section = entry.section;
      while (section.has_value()) {
        record.sections.push_back(*section);
        memory_.insert_or_assign(*section, entry.id);
        const auto found = sections_.find(*section);
        section =
            found == sections_.end() ? std::nullopt : found->second.parent;
      }
      const auto found = tree.node(entry.id);
      record.node = entry.id;
      record.parent = found.has_value() ? found->get().parent() : std::nullopt;
      record.frame = entry.frame;
      if (entry.key == nullptr) {
        record.key.reset();
      } else if (record.key != *entry.key) {
        record.key = *entry.key;
      }
      const std::span<const std::size_t> path = path_of(entry);
      record.path.assign(path.begin(), path.end());
      record.type = entry.type;
      if (entry.matched == nullptr) {
        record.matched.reset();
      } else if (record.matched != *entry.matched) {
        record.matched = *entry.matched;
      }
      record.horizon = horizon_;
    }
  }
  std::erase_if(memory_, [this](const auto& remembered) {
    return !span_of(remembered.first).has_value();
  });
  publish(tree);
}

void FocusEngine::publish(composition::Tree& tree) {
  for (const composition::NodeId id : bound_) {
    if (id == focused_) {
      continue;
    }
    if (const auto node = tree.node(id)) {
      for (const auto& binding : node->get().interactions().focused) {
        binding->release();
      }
    }
  }
  if (focused_.has_value()) {
    if (const auto node = tree.node(*focused_)) {
      for (const auto& binding : node->get().interactions().focused) {
        if (!binding->requested()) {
          binding->grant();
        }
      }
    }
  }
  requested_.clear();
  for (const composition::NodeId id : bound_) {
    if (const auto node = tree.node(id)) {
      const auto& bindings = node->get().interactions().focused;
      for (std::size_t index = 0; index < bindings.size(); ++index) {
        if (bindings[index]->requested()) {
          requested_.insert({id, index});
        }
      }
    }
  }
}

auto FocusEngine::synchronise(composition::Tree& tree) -> Synchronisation {
  const Stamp stamp{.revision = tree.revision(),
                    .visibility = tree.visibility(),
                    .geometry = tree.arrangement(),
                    .exclusive = tree.exclusive().revision()};
  const bool structural = !stamp_.has_value() || stamp_->structural(stamp);
  const bool revised =
      !stamp_.has_value() || stamp_->revision != stamp.revision;
  if (!structural && stamp_->geometry != stamp.geometry) {
    stale_ = true;
  }
  if (!structural && !pending_.has_value() && !pending_restore_.has_value() &&
      tree.focus_resets()->empty() && (focused_.has_value() || dormant_)) {
    stamp_ = stamp;
    return Synchronisation{};
  }
  stamp_ = stamp;
  const std::optional<composition::NodeId> before = focused_;
  if (structural) {
    if (!revised) {
      ++uncovered_;
    }
    ++collections_;
    collect(tree);
    confine(tree);
    requests(tree);
    restore(tree);
  } else {
    refresh(tree);
  }
  if (focused_.has_value() && !valid(*focused_)) {
    focused_ = recover(tree);
  } else if (!focused_.has_value() && !dormant_) {
    focused_ = preferred(tree, boundary()).or_else([this] {
      return first_inside(boundary());
    });
  }
  for (const composition::Namespace::ID scope : tree.focus_resets()->take()) {
    const std::optional<composition::NodeId> holder =
        scoped(tree.root(), scope);
    if (!holder.has_value()) {
      continue;
    }
    const std::optional<composition::NodeId> chosen =
        preferred(tree, *holder).or_else([&] { return first_inside(*holder); });
    if (chosen.has_value() && valid(*chosen)) {
      focused_ = chosen;
      dormant_ = false;
      pending_restore_.reset();
    }
  }
  if (pending_restore_.has_value()) {
    if (const auto restored = by_key(*pending_restore_)) {
      focused_ = restored;
      pending_restore_.reset();
    }
  }
  Synchronisation result;
  if (pending_.has_value()) {
    const auto target = advance(tree, pending_->direction);
    if (target.has_value() &&
        (!pending_->within.has_value() || inside(*target, *pending_->within))) {
      if (focused_.has_value()) {
        aim(*focused_, *target, pending_->direction);
      }
      focused_ = target;
      result.deferred = MoveOutcome::moved;
      pending_.reset();
    } else if (--pending_->frames <= 0) {
      result.deferred = MoveOutcome::blocked;
      pending_.reset();
    }
  }
  adopt(tree, focused_);
  result.changed = before != focused_;
  return result;
}

auto FocusEngine::move(composition::Tree& tree,
                       composition::MoveCommandDirection direction,
                       std::optional<composition::NodeId> keep_within)
    -> MoveOutcome {
  refresh(tree);
  pending_.reset();
  pending_restore_.reset();
  if (!focused_.has_value() || !valid(*focused_)) {
    dormant_ = false;
    const auto chosen = preferred(tree, boundary()).or_else([this] {
      return first_inside(boundary());
    });
    if (!chosen.has_value()) {
      return MoveOutcome::blocked;
    }
    adopt(tree, chosen);
    return MoveOutcome::moved;
  }
  if (tree.steer(*focused_, direction)) {
    return MoveOutcome::moved;
  }
  const auto target = advance(tree, direction);
  if (target.has_value() &&
      (!keep_within.has_value() || inside(*target, *keep_within))) {
    aim(*focused_, *target, direction);
    adopt(tree, target);
    tree.enter(*target, direction);
    return MoveOutcome::moved;
  }
  if (keep_within.has_value()) {
    std::optional<composition::NodeId> climbing = focused_;
    while (climbing.has_value() && *climbing != *keep_within) {
      if (tree.materialize(*climbing, direction)) {
        pending_ = Pending{.direction = direction,
                           .frames = deferral_frames,
                           .within = keep_within};
        return MoveOutcome::deferred;
      }
      const auto node = tree.node(*climbing);
      climbing = node.has_value() ? node->get().parent() : std::nullopt;
    }
    const auto index = index_of(*focused_);
    std::vector<composition::Rect> frames;
    std::vector<composition::NodeId> ids;
    for (const Entry& entry : entries_) {
      if (entry.id != *focused_ && eligible(entry) &&
          inside(entry.id, *keep_within)) {
        frames.push_back(entry.frame);
        ids.push_back(entry.id);
      }
    }
    if (index.has_value()) {
      if (const auto pick =
              search_.nearest(entries_[*index].frame, direction, frames)) {
        adopt(tree, ids[*pick]);
        return MoveOutcome::moved;
      }
    }
    return MoveOutcome::held;
  }
  if (target.has_value()) {
    aim(*focused_, *target, direction);
    adopt(tree, target);
    return MoveOutcome::moved;
  }
  const composition::NodeId limit = boundary();
  std::optional<composition::NodeId> current = focused_;
  while (current.has_value() && *current != limit) {
    if (tree.materialize(*current, direction)) {
      pending_ = Pending{.direction = direction, .frames = deferral_frames};
      return MoveOutcome::deferred;
    }
    const auto node = tree.node(*current);
    current = node.has_value() ? node->get().parent() : std::nullopt;
  }
  return MoveOutcome::blocked;
}

auto FocusEngine::scoped(const composition::Node& node,
                         composition::Namespace::ID scope)
    -> std::optional<composition::NodeId> {
  const auto& declared = node.interactions().focus_scope;
  if (declared.has_value() && declared->id == scope) {
    return node.id();
  }
  for (const composition::Node& child : node.children()) {
    if (const auto found = scoped(child, scope)) {
      return found;
    }
  }
  return std::nullopt;
}

auto FocusEngine::focus(composition::Tree& tree, composition::NodeId node)
    -> bool {
  refresh(tree);
  if (!valid(node)) {
    return false;
  }
  dormant_ = false;
  pending_.reset();
  pending_restore_.reset();
  adopt(tree, node);
  return true;
}

auto FocusEngine::clipped_out(const composition::Tree& tree,
                              const composition::Node& node,
                              composition::Point point) -> bool {
  std::optional<composition::NodeId> parent = node.parent();
  while (parent.has_value()) {
    const auto above = tree.node(*parent);
    if (!above.has_value()) {
      return false;
    }
    const std::optional<composition::PresentedFrame>& presented =
        above->get().presented();
    if (presented.has_value() && presented->clip_shape.has_value() &&
        !presented->clip_shape->contains(point)) {
      return true;
    }
    parent = above->get().parent();
  }
  return false;
}

auto FocusEngine::hit(const composition::Tree& tree, composition::Point point)
    -> std::optional<composition::NodeId> {
  refresh(tree);
  std::optional<composition::NodeId> chosen;
  for (const Entry& entry : entries_) {
    if (!eligible(entry)) {
      continue;
    }
    const auto node = tree.node(entry.id);
    if (!node.has_value()) {
      continue;
    }
    const std::optional<composition::PresentedFrame>& presented =
        node->get().presented();
    const bool contains =
        (presented.has_value() ? presented->covers(point)
                               : entry.frame.contains(point.x, point.y)) &&
        !clipped_out(tree, node->get(), point);
    if (contains && (!chosen.has_value() || !composition::PaintOrder::above(
                                                tree, *chosen, entry.id))) {
      chosen = entry.id;
    }
  }
  return chosen;
}

}
