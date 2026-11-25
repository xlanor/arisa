#include <arisa/composition/tree.hpp>

#include <arisa/composition/context.hpp>
#include <arisa/composition/feedback_enablement.hpp>
#include <arisa/composition/layer.hpp>
#include <arisa/composition/preference_observer.hpp>

#include <composition/node_lifecycle.hpp>
#include <composition/reconciler.hpp>

#include <arisa/motion/transaction_stack.hpp>
#include <arisa/motion/transition.hpp>

#include <algorithm>
#include <cassert>
#include <limits>
#include <ranges>
#include <utility>
#include <vector>

namespace arisa::composition {

class Tree::RootContent final : public ElementContent {
 public:
  auto type() const noexcept -> ElementType override {
    return ElementType::of<RootContent>();
  }
};

Tree::Tree(Element content, OwnerCheck owner,
           std::optional<RootServices> services)
    : owner_(std::move(owner)),
      services_(services),
      pending_(std::move(content)),
      root_(std::make_unique<Node>(
          *this, allocate(), std::make_unique<RootContent>(), std::nullopt,
          std::nullopt, std::nullopt, 0)) {
  root_->lifecycle().appear();
  dirty_.insert(root_->id());
}

Tree::~Tree() {
  std::map<NodeId, std::vector<std::unique_ptr<Node>>> drained =
      std::move(recycled_);
  recycled_.clear();
  drained.clear();
}

auto Tree::recyclable() const noexcept -> std::size_t {
  std::size_t total = 0;
  for (const auto& entry : recycled_) {
    total += entry.second.size();
  }
  return total;
}

void Tree::reconcile() {
  assert(owner_());
  if (reconciler_ == nullptr) {
    reconciler_ = std::make_unique<Reconciler>(*this);
  }
  reconciler_->run(false);
  deferred_ = dirty_;
  settle_preferences();
}

void Tree::settle_geometry() {
  assert(owner_());
  if (reconciler_ == nullptr) {
    reconciler_ = std::make_unique<Reconciler>(*this);
  }
  reconciler_->run(true);
  deferred_.insert(dirty_.begin(), dirty_.end());
  settle_preferences();
}

auto Tree::geometry_dirty() const -> bool {
  return anchors_moved_ || std::ranges::any_of(dirty_, [this](NodeId id) {
           return !deferred_.contains(id);
         });
}

void Tree::settle_preferences() {
  for (std::size_t round = 0; round < preference_rounds; ++round) {
    if (!resolve_preferences()) {
      return;
    }
    reconciler_->run(true);
    deferred_.insert(dirty_.begin(), dirty_.end());
  }
}

auto Tree::resolve_preferences() -> bool {
  anchors_moved_ = false;
  const std::uint64_t epoch = preference_epoch_;
  bool released = false;
  releasing_ = true;
  for (const NodeId id : preference_readers_) {
    const auto found = lookup(id);
    if (!found.has_value()) {
      continue;
    }
    Node& node = found->get();
    if (!node.active() ||
        (node.preference_resolved_ != 0 &&
         node.preference_stamp_ <= node.preference_resolved_)) {
      continue;
    }
    node.preference_resolved_ = epoch;
    ++preference_resolutions_;
    const std::size_t before = dirty_.size();
    for (const std::shared_ptr<PreferenceObserver>& observer :
         node.preference_observers_) {
      if (observer->resolve(node)) {
        delivering_.push_back(observer);
      }
    }
    if (node.content_->reads_preferences()) {
      Context context{*this, node};
      node.content_->resolve(context, node);
    }
    if (dirty_.size() > before) {
      geometry_dirtied_.insert(id);
    }
  }
  ++preference_epoch_;
  for (std::size_t index = 0; index < delivering_.size(); ++index) {
    const std::shared_ptr<PreferenceObserver> observer = delivering_[index];
    observer->deliver();
  }
  delivering_.clear();
  releasing_ = false;
  released = std::ranges::any_of(
      dirty_, [this](NodeId id) { return !deferred_.contains(id); });
  return released;
}

void Tree::touch_preferences(Node& node) {
  std::optional<std::reference_wrapper<Node>> at = node;
  while (at.has_value()) {
    Node& current = at->get();
    if (current.preferring_ && current.preference_stamp_ == preference_epoch_) {
      return;
    }
    current.preferring_ = true;
    current.preference_stamp_ = preference_epoch_;
    at = current.parent_.has_value() ? lookup(*current.parent_) : std::nullopt;
  }
}

void Tree::enroll(const Node& node) {
  if (node.content_->reads_preferences() ||
      !node.preference_observers_.empty()) {
    preference_readers_.insert(node.id_);
  } else {
    preference_readers_.erase(node.id_);
  }
  if (node.anchored_) {
    scroll_watchers_.insert(node.id_);
  } else {
    scroll_watchers_.erase(node.id_);
  }
  if (observes_geometry(node)) {
    geometry_watchers_.insert(node.id_);
  } else {
    geometry_watchers_.erase(node.id_);
  }
}

auto Tree::scrolled_frame(const Node& node) const -> Rect {
  Rect frame = node.frame_;
  std::optional<NodeId> parent = node.parent_;
  while (parent.has_value()) {
    const auto found = registry_.find(*parent);
    if (found == registry_.end()) {
      break;
    }
    const Node& above = found->second.get();
    if (!above.interactions_->effects.projections.empty()) {
      frame = projected(above, frame);
    }
    if (above.interactions_->scrolling.container.has_value()) {
      frame.x -= above.scroll_presentation_.offset.x;
      frame.y -= above.scroll_presentation_.offset.y;
    }
    parent = above.parent_;
  }
  return frame;
}

auto Tree::projected(const Node& above, Rect frame) -> Rect {
  const Size size{.width = above.frame_.width, .height = above.frame_.height};
  for (const ScrollEffects::Projection& projection :
       above.interactions_->effects.projections) {
    const graphics::ProjectionTransform transform = projection(size);
    if (!transform.is_affine() || transform.is_identity()) {
      continue;
    }
    const graphics::AffineTransform affine = transform.affine();
    const float left = frame.x - above.frame_.x;
    const float top = frame.y - above.frame_.y;
    float low_x = std::numeric_limits<float>::max();
    float low_y = std::numeric_limits<float>::max();
    float high_x = std::numeric_limits<float>::lowest();
    float high_y = std::numeric_limits<float>::lowest();
    for (const auto& [x, y] :
         {std::pair{left, top}, std::pair{left + frame.width, top},
          std::pair{left, top + frame.height},
          std::pair{left + frame.width, top + frame.height}}) {
      const float moved_x = (affine.a() * x) + (affine.c() * y) + affine.tx();
      const float moved_y = (affine.b() * x) + (affine.d() * y) + affine.ty();
      low_x = std::min(low_x, moved_x);
      low_y = std::min(low_y, moved_y);
      high_x = std::max(high_x, moved_x);
      high_y = std::max(high_y, moved_y);
    }
    frame = Rect{.x = above.frame_.x + low_x,
                 .y = above.frame_.y + low_y,
                 .width = high_x - low_x,
                 .height = high_y - low_y};
  }
  return frame;
}

auto Tree::inside(const Node& node, NodeId ancestor) const -> bool {
  std::optional<NodeId> parent = node.parent_;
  while (parent.has_value()) {
    if (*parent == ancestor) {
      return true;
    }
    const auto above = registry_.find(*parent);
    parent =
        above == registry_.end() ? std::nullopt : above->second.get().parent_;
  }
  return false;
}

void Tree::follow_scroll(NodeId scroller) {
  for (const NodeId id : scroll_watchers_) {
    const auto found = lookup(id);
    if (!found.has_value()) {
      continue;
    }
    Node& node = found->get();
    if (inside(node, scroller)) {
      touch_preferences(node);
      anchors_moved_ = true;
    }
  }
  renotify(scroller, true);
}

void Tree::reproject(NodeId projector) {
  if (!geometry_watchers_.empty()) {
    renotify(projector, false);
  }
}

void Tree::renotify(NodeId ancestor, bool scrolled) {
  for (const NodeId id : geometry_watchers_) {
    const auto found = lookup(id);
    if (!found.has_value() || !found->get().active()) {
      continue;
    }
    Node& node = found->get();
    if (!inside(node, ancestor)) {
      continue;
    }
    if (deferring_ > 0) {
      if (std::ranges::find(placed_, id) == placed_.end()) {
        placed_.push_back(id);
      }
      continue;
    }
    notify_geometry(node, scrolled);
  }
}

auto Tree::inherited(const Node& node, Identity key) const
    -> const EnvironmentOverride::Value* {
  std::optional<NodeId> parent = node.parent_;
  while (parent.has_value()) {
    const auto found = registry_.find(*parent);
    if (found == registry_.end()) {
      return nullptr;
    }
    const Node& above = found->second.get();
    const auto entry =
        std::ranges::find(above.overrides_, key, &EnvironmentOverride::key);
    if (entry != above.overrides_.end()) {
      return &entry->value();
    }
    parent = above.parent_;
  }
  return nullptr;
}

auto Tree::environment_values(const Node& node) const -> EnvironmentValues {
  std::vector<EnvironmentValues::Entry> found;
  const Node* current = &node;
  while (current != nullptr) {
    for (const EnvironmentOverride& entry : current->overrides_) {
      if (std::ranges::find(found, entry.key(),
                            &EnvironmentValues::Entry::key) == found.end()) {
        found.push_back(EnvironmentValues::Entry{.key = entry.key(),
                                                 .value = entry.shared()});
      }
    }
    if (!current->parent_.has_value()) {
      break;
    }
    const auto above = registry_.find(*current->parent_);
    current = above == registry_.end() ? nullptr : &above->second.get();
  }
  return EnvironmentValues{std::move(found)};
}

auto Tree::transformed(const Node& node,
                       std::span<const EnvironmentTransform> transforms) const
    -> std::vector<EnvironmentOverride> {
  std::vector<EnvironmentOverride> made;
  for (const EnvironmentTransform& transform :
       transforms | std::views::reverse) {
    const auto prior =
        std::ranges::find(made, transform.key(), &EnvironmentOverride::key);
    if (prior != made.end()) {
      EnvironmentOverride next = transform.apply(&prior->value());
      *prior = std::move(next);
      continue;
    }
    made.push_back(transform.apply(inherited(node, transform.key())));
  }
  return made;
}

auto Tree::transforms(const Node& node, Identity key) noexcept -> bool {
  return std::ranges::any_of(node.transforms_,
                             [key](const EnvironmentTransform& transform) {
                               return transform.key() == key;
                             });
}

auto Tree::rematerialize(Node& node, Identity key) -> bool {
  std::vector<EnvironmentOverride> made = transformed(node, node.transforms_);
  const auto fresh = std::ranges::find(made, key, &EnvironmentOverride::key);
  if (fresh == made.end()) {
    return false;
  }
  const auto held =
      std::ranges::find(node.overrides_, key, &EnvironmentOverride::key);
  if (held == node.overrides_.end()) {
    node.overrides_.push_back(std::move(*fresh));
    repaint(node);
    return true;
  }
  if (held->same_value(*fresh)) {
    return false;
  }
  *held = std::move(*fresh);
  repaint(node);
  return true;
}

auto Tree::focus_owner(const Node& node) const -> std::optional<NodeId> {
  const Node* current = &node;
  while (current != nullptr) {
    if (current->interactions_->focusable.value_or(
            current->content_->activatable())) {
      return current->id_;
    }
    if (!current->parent_.has_value()) {
      return std::nullopt;
    }
    const auto found = registry_.find(*current->parent_);
    current = found == registry_.end() ? nullptr : &found->second.get();
  }
  return std::nullopt;
}

auto Tree::contains_focus(const Node& node) const -> bool {
  std::optional<NodeId> at = focused_;
  while (at.has_value()) {
    if (*at == node.id_) {
      return true;
    }
    const auto found = registry_.find(*at);
    if (found == registry_.end()) {
      return false;
    }
    at = found->second.get().parent_;
  }
  return false;
}

void Tree::release_focus_readers(std::optional<NodeId> owner) {
  if (!owner.has_value()) {
    return;
  }
  const auto found = focus_readers_.find(*owner);
  if (found == focus_readers_.end()) {
    return;
  }
  for (const NodeId reader : found->second) {
    if (const auto node = lookup(reader);
        node.has_value() && node->get().active()) {
      dirty_.insert(reader);
      repaint(node->get());
    }
  }
  focus_readers_.erase(found);
}

void Tree::mark_dirty(NodeId node) {
  assert(owner_());
  if (!registry_.contains(node)) {
    return;
  }
  dirty_.insert(node);
  if (releasing_) {
    deferred_.erase(node);
  }
  const auto current = motion::TransactionStack::current();
  if (!current.has_value()) {
    return;
  }
  const motion::Transaction& transaction = current->get();
  transaction.attach(completions_);
  transactions_.insert_or_assign(node, transaction);
  if (ambient(transaction)) {
    candidate_ = transaction;
  }
}

auto Tree::ambient(const motion::Transaction& transaction) noexcept -> bool {
  const std::optional<motion::Animation>& animation = transaction.animation();
  if (!animation.has_value()) {
    return false;
  }
  const std::optional<motion::Animation::Repetition>& repetition =
      animation->repetition();
  return !repetition.has_value() || repetition->count.has_value();
}

auto Tree::presentation(const Node& node) -> motion::Presentation& {
  return lookup(node.id())->get().presentation_.edit();
}

auto Tree::transaction(const Node& node) const noexcept
    -> const motion::Transaction& {
  if (node.transaction_.has_value()) {
    return *node.transaction_;
  }
  if (pass_.has_value()) {
    return *pass_;
  }
  return none_;
}

void Tree::close_transactions() noexcept {
  if (held_epoch_ != paint_epoch_) {
    held_epoch_ = paint_epoch_;
    touched_.insert(held_.begin(), held_.end());
    held_.clear();
  }
  for (const NodeId id : touched_) {
    ++examined_transactions_;
    const auto found = lookup(id);
    if (!found.has_value()) {
      continue;
    }
    Node& node = found->get();
    if (node.transaction_.has_value() && node.presentation_->suspended()) {
      held_.insert(id);
      continue;
    }
    node.transaction_.reset();
  }
  touched_.clear();
  pass_.reset();
}

auto Tree::run_completions() -> std::size_t { return completions_->drain(); }

auto Tree::completions_pending() const noexcept -> bool {
  return completions_->pending();
}

auto Tree::node(NodeId id) const
    -> std::optional<std::reference_wrapper<const Node>> {
  const auto found = registry_.find(id);
  if (found == registry_.end()) {
    return std::nullopt;
  }
  return std::cref(found->second.get());
}

auto Tree::find(const ElementKey& key) const -> std::optional<NodeId> {
  return indexed(key, root_->id_);
}

auto Tree::find(const ElementKey& key, NodeId within) const
    -> std::optional<NodeId> {
  if (!registry_.contains(within)) {
    return std::nullopt;
  }
  return indexed(key, within);
}

auto Tree::indexed(const ElementKey& key, NodeId top) const
    -> std::optional<NodeId> {
  const auto [first, last] = keyed_.equal_range(key.hash());
  std::optional<NodeId> chosen;
  std::size_t matches = 0;
  for (auto at = first; at != last; ++at) {
    const auto found = registry_.find(at->second);
    if (found == registry_.end()) {
      continue;
    }
    const Node& candidate = found->second.get();
    if (!candidate.identified_by(key) || !reachable(candidate, top)) {
      continue;
    }
    chosen = candidate.id_;
    ++matches;
  }
  if (matches < 2) {
    return chosen;
  }
  ++searches_;
  const auto held = registry_.find(top);
  return held == registry_.end() ? std::nullopt
                                 : search(held->second.get(), key);
}

auto Tree::reachable(const Node& node, NodeId top) const -> bool {
  const Node* current = &node;
  while (true) {
    if (!current->active()) {
      return false;
    }
    if (current->id_ == top) {
      return true;
    }
    if (!current->parent_.has_value()) {
      return false;
    }
    const auto found = registry_.find(*current->parent_);
    if (found == registry_.end()) {
      return false;
    }
    const Node& parent = found->second.get();
    if (std::ranges::none_of(parent.children(), [current](const Node& child) {
          return &child == current;
        })) {
      return false;
    }
    current = &parent;
  }
}

void Tree::index(const Node& node) {
  if (node.key_.has_value()) {
    index(node.key_->hash(), node.id_);
  }
  if (node.alias_.has_value()) {
    index(node.alias_->hash(), node.id_);
  }
}

void Tree::index(std::size_t hash, NodeId id) {
  if (spare_keys_.empty()) {
    keyed_.emplace(hash, id);
    return;
  }
  KeyIndex::node_type entry = std::move(spare_keys_.back());
  spare_keys_.pop_back();
  entry.key() = hash;
  entry.mapped() = id;
  keyed_.insert(std::move(entry));
}

void Tree::unindex(const Node& node) noexcept {
  if (node.key_.has_value()) {
    unindex(node.key_->hash(), node.id_);
  }
  if (node.alias_.has_value()) {
    unindex(node.alias_->hash(), node.id_);
  }
}

void Tree::unindex(std::size_t hash, NodeId id) noexcept {
  const auto [first, last] = keyed_.equal_range(hash);
  for (auto at = first; at != last; ++at) {
    if (at->second == id) {
      KeyIndex::node_type entry = keyed_.extract(at);
      if (spare_keys_.size() < spare_entry_limit) {
        spare_keys_.push_back(std::move(entry));
      }
      return;
    }
  }
}

void Tree::label(Node& node, std::optional<ElementKey> key,
                 std::optional<ElementKey> alias) {
  unindex(node);
  node.key_ = std::move(key);
  node.alias_ = std::move(alias);
  index(node);
}

auto Tree::activate(NodeId id) -> bool { return activate(id, std::nullopt); }

auto Tree::activate(NodeId id, std::optional<Point> at) -> bool {
  assert(owner_());
  const auto found = lookup(id);
  if (!found.has_value() || !found->get().active()) {
    return false;
  }
  Node& node = found->get();
  Context context{*this, node};
  return node.content_->activate(context, node, at);
}

auto Tree::press(NodeId id, const KeyPress& press) -> bool {
  assert(owner_());
  const auto found = lookup(id);
  if (!found.has_value() || !found->get().active()) {
    return false;
  }
  Node& node = found->get();
  Context context{*this, node};
  return node.content_->press(context, node, press);
}

auto Tree::steer(NodeId id, MoveCommandDirection direction) -> bool {
  assert(owner_());
  const auto found = lookup(id);
  if (!found.has_value() || !found->get().active()) {
    return false;
  }
  Node& node = found->get();
  Context context{*this, node};
  return node.content_->steer(context, node, direction);
}

void Tree::enter(NodeId id, MoveCommandDirection direction) {
  assert(owner_());
  const auto found = lookup(id);
  if (!found.has_value() || !found->get().active()) {
    return;
  }
  Node& node = found->get();
  Context context{*this, node};
  node.content_->enter(context, node, direction);
}

void Tree::assign_frame(NodeId id, Rect frame) {
  const auto found = lookup(id);
  if (!found.has_value()) {
    return;
  }
  Node& node = found->get();
  if (node.frame_ != frame) {
    node.frame_ = frame;
    repaint(node);
    if (node.anchored_) {
      touch_preferences(node);
      anchors_moved_ = true;
    }
  }
  if (!observes_geometry(node)) {
    return;
  }
  if (deferring_ > 0) {
    if (std::ranges::find(placed_, id) == placed_.end()) {
      placed_.push_back(id);
    }
    return;
  }
  notify_geometry(node);
}

auto Tree::observes_geometry(const Node& node) noexcept -> bool {
  return !node.interactions_->geometry.empty() ||
         node.content_->reads_geometry();
}

auto Tree::named_spaces(Node& node) const -> GeometryProxy::Spaces {
  std::shared_ptr<std::vector<GeometryProxy::NamedSpace>>& cache = node.spaces_;
  std::size_t count = 0;
  bool same = cache != nullptr;
  bool moved = false;
  for (const Node* current = &node; current != nullptr;) {
    if (const auto& name = current->interactions_->coordinate_space) {
      if (same && (count >= cache->size() || (*cache)[count].name != *name)) {
        same = false;
      }
      if (same && (*cache)[count].frame != scrolled_frame(*current)) {
        moved = true;
      }
      ++count;
    }
    const auto above = current->parent_.has_value()
                           ? this->node(*current->parent_)
                           : std::nullopt;
    current = above.has_value() ? &above->get() : nullptr;
  }
  if (count == 0) {
    cache.reset();
    return nullptr;
  }
  same = same && cache->size() == count;
  if (same && !moved) {
    return cache;
  }
  if (!same || cache.use_count() > 1) {
    cache = std::make_shared<std::vector<GeometryProxy::NamedSpace>>();
    cache->reserve(count);
  }
  std::size_t index = 0;
  for (const Node* current = &node; current != nullptr;) {
    if (const auto& name = current->interactions_->coordinate_space) {
      const Rect frame = scrolled_frame(*current);
      if (index < cache->size()) {
        (*cache)[index].frame = frame;
      } else {
        cache->push_back(
            GeometryProxy::NamedSpace{.name = *name, .frame = frame});
      }
      ++index;
    }
    const auto above = current->parent_.has_value()
                           ? this->node(*current->parent_)
                           : std::nullopt;
    current = above.has_value() ? &above->get() : nullptr;
  }
  return cache;
}

auto Tree::scroll_viewport(const Node& node) const -> std::optional<Rect> {
  std::optional<NodeId> parent = node.parent_;
  while (parent.has_value()) {
    const auto found = registry_.find(*parent);
    if (found == registry_.end()) {
      return std::nullopt;
    }
    const Node& above = found->second.get();
    if (above.interactions_->scrolling.container.has_value()) {
      return scrolled_frame(above);
    }
    parent = above.parent_;
  }
  return std::nullopt;
}

auto Tree::scroll_viewports(const Node& node) const
    -> GeometryProxy::ScrollViews {
  GeometryProxy::ScrollViews found;
  std::optional<NodeId> parent = node.parent_;
  while (parent.has_value() &&
         (!found.horizontal.has_value() || !found.vertical.has_value())) {
    const auto entry = registry_.find(*parent);
    if (entry == registry_.end()) {
      break;
    }
    const Node& above = entry->second.get();
    if (const auto& container = above.interactions_->scrolling.container) {
      if (container->axes.contains(Axis::horizontal) &&
          !found.horizontal.has_value()) {
        found.horizontal = scrolled_frame(above);
      }
      if (container->axes.contains(Axis::vertical) &&
          !found.vertical.has_value()) {
        found.vertical = scrolled_frame(above);
      }
    }
    parent = above.parent_;
  }
  return found;
}

void Tree::notify_geometry(Node& node, bool scrolled) {
  const GeometryProxy proxy{
      scrolled_frame(node),
      scroll_viewport(node),
      scroll_viewports(node),
      named_spaces(node),
      reserved_regions_,
      node.layout_slot_ != nullptr ? node.layout_slot_->layout_direction()
                                   : LayoutDirection::left_to_right,
      GeometryProxy::Surroundings{.safe_area = node.safe_area_,
                                  .container = container_corners(node)}};
  const std::size_t before = dirty_.size();
  releasing_ = true;
  for (const std::shared_ptr<GeometryObserver>& observer :
       node.interactions_->geometry) {
    observer->notify(proxy);
  }
  if (node.content_->reads_geometry()) {
    Context context{*this, node};
    node.content_->place(context, proxy);
  }
  releasing_ = false;
  if (dirty_.size() > before && !scrolled) {
    geometry_dirtied_.insert(node.id_);
  }
}

auto Tree::defer_geometry() -> support::ScopedCallback {
  ++deferring_;
  return support::ScopedCallback{[this]() noexcept {
    if (--deferring_ == 0) {
      flush_geometry();
    }
  }};
}

void Tree::flush_geometry() {
  for (std::size_t index = 0; index < placed_.size(); ++index) {
    if (const auto found = lookup(placed_[index]); found.has_value()) {
      notify_geometry(found->get());
    }
  }
  placed_.clear();
}

auto Tree::geometry_loops(std::size_t frames) -> std::vector<NodeId> {
  std::vector<NodeId> looping;
  std::erase_if(geometry_streaks_, [this](const auto& entry) {
    return !geometry_dirtied_.contains(entry.first);
  });
  for (const NodeId id : geometry_dirtied_) {
    const std::size_t streak = ++geometry_streaks_[id];
    if (streak == frames) {
      looping.push_back(id);
    }
  }
  geometry_dirtied_.clear();
  return looping;
}

void Tree::highlight(std::optional<NodeId> focused,
                     std::optional<NodeId> pressed) {
  const auto touch = [this](std::optional<NodeId> id) {
    if (!id.has_value()) {
      return;
    }
    const auto found = lookup(*id);
    if (found.has_value() && found->get().highlight_reads_ &&
        found->get().active()) {
      dirty_.insert(*id);
    }
  };
  if (focused != focused_) {
    touch(focused_);
    touch(focused);
    release_focus_readers(focused_);
    release_focus_readers(focused);
    for (const NodeId reader : focus_within_readers_) {
      if (const auto node = lookup(reader);
          node.has_value() && node->get().active()) {
        dirty_.insert(reader);
      }
    }
    focused_ = focused;
  }
  if (pressed != pressed_) {
    touch(pressed_);
    touch(pressed);
    pressed_ = pressed;
  }
  resolve_focused_values();
}

void Tree::hover(std::optional<NodeId> hovered) {
  if (hovered == hovered_) {
    return;
  }
  hovered_ = hovered;
  for (const NodeId reader : hover_readers_) {
    if (const auto node = lookup(reader);
        node.has_value() && node->get().active()) {
      dirty_.insert(reader);
    }
  }
}

auto Tree::contains_hover(const Node& node) const -> bool {
  std::optional<NodeId> at = hovered_;
  while (at.has_value()) {
    if (*at == node.id_) {
      return true;
    }
    const auto found = registry_.find(*at);
    if (found == registry_.end()) {
      return false;
    }
    at = found->second.get().parent_;
  }
  return false;
}

void Tree::collect_scene_values(
    const Node& node,
    std::flat_map<Identity, std::shared_ptr<const EnvironmentOverride>>& found)
    const {
  if (node.parent_.has_value() && !node.active()) {
    return;
  }
  for (const auto& value : node.interactions_->scene_values) {
    found.try_emplace(value->key(), value);
  }
  for (const Node& child : node.children()) {
    collect_scene_values(child, found);
  }
}

void Tree::resolve_focused_values() {
  if (focused_value_readers_.empty() && focused_values_readers_.empty()) {
    return;
  }
  if (resolved_once_ && resolved_focus_ == focused_ &&
      resolved_revision_ == revision_) {
    return;
  }
  resolved_once_ = true;
  resolved_focus_ = focused_;
  resolved_revision_ = revision_;
  std::flat_map<Identity, std::shared_ptr<const EnvironmentOverride>> next;
  std::optional<NodeId> at = focused_;
  while (at.has_value()) {
    const auto found = registry_.find(*at);
    if (found == registry_.end()) {
      break;
    }
    const Node& node = found->second.get();
    for (const auto& value : node.interactions_->focused_values) {
      next.try_emplace(value->key(), value);
    }
    at = node.parent_;
  }
  collect_scene_values(*root_, next);
  const auto touch_readers = [this](Identity key) {
    const auto readers = focused_value_readers_.find(key);
    if (readers == focused_value_readers_.end()) {
      return;
    }
    for (const NodeId reader : readers->second) {
      if (const auto node = lookup(reader);
          node.has_value() && node->get().active()) {
        dirty_.insert(reader);
      }
    }
  };
  bool any = false;
  for (const auto& [key, value] : next) {
    const auto before = focused_values_.find(key);
    if (before == focused_values_.end() ||
        !before->second->same_value(*value)) {
      touch_readers(key);
      any = true;
    }
  }
  for (const auto& [key, value] : focused_values_) {
    if (!next.contains(key)) {
      touch_readers(key);
      any = true;
    }
  }
  if (any) {
    for (const NodeId reader : focused_values_readers_) {
      if (const auto node = lookup(reader);
          node.has_value() && node->get().active()) {
        dirty_.insert(reader);
      }
    }
  }
  focused_values_ = std::move(next);
}

auto Tree::focused_snapshot(NodeId reader)
    -> std::flat_map<Identity, std::shared_ptr<const EnvironmentOverride>> {
  focused_values_readers_.insert(reader);
  return focused_values_;
}

auto Tree::focused_lookup(Identity key, NodeId reader)
    -> const EnvironmentOverride::Value* {
  focused_value_readers_[key].insert(reader);
  const auto found = focused_values_.find(key);
  if (found == focused_values_.end()) {
    return nullptr;
  }
  return &found->second->value();
}

void Tree::present(NodeId id, PresentedFrame frame) {
  if (const auto found = lookup(id)) {
    Node& node = found->get();
    const std::optional<PresentedFrame>& held = *node.presented_;
    if (held != frame) {
      const bool first = !held.has_value();
      const bool flipped =
          first ? !frame.visible : held->visible != frame.visible;
      if (flipped) {
        ++visibility_;
      }
      if (first || flipped || arranges(node)) {
        ++arrangement_;
      }
      node.presented_.edit() = frame;
      ++geometry_;
    }
  }
}

auto Tree::arranges(const Node& node) noexcept -> bool {
  const Interactions& declared = *node.interactions_;
  return declared.focusable.value_or(node.content_->activatable()) ||
         declared.focus_section || declared.focus_scope.has_value() ||
         declared.scrolling.container.has_value() ||
         node.content_as<Layer::Content>().has_value();
}

auto Tree::materialize(NodeId id, MoveCommandDirection direction) -> bool {
  assert(owner_());
  const auto found = lookup(id);
  if (!found.has_value() || !found->get().active()) {
    return false;
  }
  Node& node = found->get();
  Context context{*this, node};
  return node.content_->materialize(context, node, direction);
}

auto Tree::reveal(NodeId id, const Rect& visible) -> bool {
  assert(owner_());
  const auto found = lookup(id);
  if (!found.has_value() || !found->get().active()) {
    return false;
  }
  Node& node = found->get();
  Context context{*this, node};
  return node.content_->reveal(context, node, visible);
}

auto Tree::locate(NodeId id, const ElementKey& key) -> std::optional<Rect> {
  assert(owner_());
  const auto found = lookup(id);
  if (!found.has_value() || !found->get().active()) {
    return std::nullopt;
  }
  Node& node = found->get();
  Context context{*this, node};
  return node.content_->locate(context, node, key);
}

auto Tree::page_stop(NodeId id, float position) -> std::optional<float> {
  assert(owner_());
  const auto found = lookup(id);
  if (!found.has_value() || !found->get().active()) {
    return std::nullopt;
  }
  Node& node = found->get();
  Context context{*this, node};
  return node.content_->page_stop(context, node, position);
}

void Tree::present_scroll(NodeId id, ScrollPresentation presentation) {
  if (const auto found = lookup(id);
      found.has_value() && found->get().scroll_presentation_ != presentation) {
    const bool moved =
        found->get().scroll_presentation_.offset != presentation.offset;
    if (moved) {
      ++arrangement_;
    }
    found->get().scroll_presentation_ = presentation;
    repaint(found->get());
    if (moved && (!scroll_watchers_.empty() || !geometry_watchers_.empty())) {
      follow_scroll(id);
    }
  }
}

void Tree::repaint(NodeId id) {
  if (const auto found = lookup(id)) {
    repaint(found->get());
  }
}

void Tree::repaint(Node& node) {
  node.own_paint_stamp_ = paint_epoch_;
  std::optional<std::reference_wrapper<Node>> at = node;
  while (at.has_value() && at->get().paint_stamp_ != paint_epoch_) {
    at->get().paint_stamp_ = paint_epoch_;
    const std::optional<NodeId> parent = at->get().parent_;
    at = parent.has_value() ? lookup(*parent) : std::nullopt;
  }
}

void Tree::repaint_subtree(Node& node) {
  repaint(node);
  for (const std::unique_ptr<Node>& child : node.children_) {
    child->paint_stamp_ = 0;
    child->own_paint_stamp_ = 0;
    repaint_subtree(*child);
  }
}

void Tree::assign_paint_slot(NodeId id, std::uint32_t slot) {
  if (const auto found = lookup(id)) {
    found->get().paint_slot_ = slot;
  }
}

void Tree::assign_link_frames(NodeId id, std::span<const LinkFrame> frames) {
  if (const auto found = lookup(id);
      found.has_value() &&
      !std::ranges::equal(found->get().link_frames_, frames)) {
    found->get().link_frames_.assign(frames.begin(), frames.end());
  }
}

void Tree::assign_published(NodeId id, std::optional<Preference> published) {
  const auto found = lookup(id);
  if (!found.has_value()) {
    return;
  }
  Node& node = found->get();
  if (!node.published_.has_value() && !published.has_value()) {
    return;
  }
  node.published_ = std::move(published);
  node.anchored_ = std::ranges::any_of(node.preferences_,
                                       [](const Preference& entry) {
                                         return entry.anchored();
                                       }) ||
                   (node.published_.has_value() && node.published_->anchored());
  enroll(node);
  touch_preferences(node);
  anchors_moved_ = true;
}

void Tree::assign_content_size(NodeId id, std::optional<Size> size) {
  if (const auto found = lookup(id);
      found.has_value() && found->get().content_size_ != size) {
    found->get().content_size_ = size;
    repaint(found->get());
  }
}

void Tree::assign_content_frame(NodeId id, std::optional<Rect> frame) {
  if (const auto found = lookup(id);
      found.has_value() && found->get().content_frame_ != frame) {
    found->get().content_frame_ = frame;
    repaint(found->get());
  }
}

void Tree::assign_safe_area(NodeId id, theme::Edges insets) {
  if (const auto found = lookup(id)) {
    found->get().safe_area_ = insets;
  }
}

void Tree::assign_enclosure(
    NodeId id, std::optional<ContainerCorners::Enclosure> enclosure) {
  if (const auto found = lookup(id)) {
    found->get().enclosure_ = enclosure;
  }
}

auto Tree::container_corners(const Node& node) const
    -> std::optional<ContainerCorners> {
  if (!node.enclosure_.has_value()) {
    return std::nullopt;
  }
  const auto found = registry_.find(node.enclosure_->container);
  if (found == registry_.end()) {
    return std::nullopt;
  }
  return ContainerCorners{
      scrolled_frame(found->second.get()), node.enclosure_->radii,
      node.layout_slot_ != nullptr ? node.layout_slot_->layout_direction()
                                   : LayoutDirection::left_to_right};
}

void Tree::assign_content_insets(NodeId id, theme::Edges insets) {
  if (const auto found = lookup(id)) {
    found->get().content_insets_ = insets;
  }
}

void Tree::declare_reserved_regions(std::vector<ReservedRegion> regions) {
  reserved_regions_ = regions.empty()
                          ? nullptr
                          : std::make_shared<const std::vector<ReservedRegion>>(
                                std::move(regions));
  std::vector<NodeId> observing;
  for (const auto& [id, node] : registry_) {
    if (observes_geometry(node.get())) {
      observing.push_back(id);
    }
  }
  for (const NodeId id : observing) {
    if (deferring_ > 0) {
      if (std::ranges::find(placed_, id) == placed_.end()) {
        placed_.push_back(id);
      }
    } else if (const auto found = lookup(id)) {
      notify_geometry(found->get());
    }
  }
}

void Tree::request_feedback(NodeId node, SensoryFeedback feedback,
                            FeedbackOrigin origin) {
  feedback_.push_back(
      FeedbackRequest{.node = node, .feedback = feedback, .origin = origin});
}

auto Tree::take_feedback() -> std::vector<SensoryFeedback> {
  std::vector<SensoryFeedback> admitted;
  for (const FeedbackRequest& request : std::exchange(feedback_, {})) {
    if (admits(request)) {
      admitted.push_back(request.feedback);
    }
  }
  return admitted;
}

auto Tree::admits(const FeedbackRequest& request) const -> bool {
  std::optional<bool> enabled;
  std::optional<bool> system;
  std::optional<NodeId> current = request.node;
  bool reached = false;
  while (current.has_value()) {
    const auto found = node(*current);
    if (!found.has_value()) {
      break;
    }
    reached = true;
    const Node& at = found->get();
    if (!enabled.has_value()) {
      if (const auto value =
              at.override_of(FeedbackEnablement::is_sensory_feedback_enabled)) {
        enabled = value->get();
      }
    }
    if (!system.has_value()) {
      if (const auto value =
              at.override_of(FeedbackEnablement::is_system_feedback_enabled)) {
        system = value->get();
      }
    }
    current = at.parent();
  }
  return reached && enabled.value_or(true) &&
         (request.origin == FeedbackOrigin::application ||
          system.value_or(true));
}

void Tree::publish(EnvironmentOverride entry) {
  assert(owner_());
  const Identity key = entry.key();
  std::vector<EnvironmentOverride>& overrides = root_->overrides_;
  const auto existing =
      std::ranges::find(overrides, key, &EnvironmentOverride::key);
  if (existing != overrides.end()) {
    if (existing->same_value(entry)) {
      return;
    }
    *existing = std::move(entry);
  } else {
    overrides.push_back(std::move(entry));
  }
  ++revision_;
  repaint(*root_);
  for (const std::reference_wrapper<Node> node :
       std::views::values(registry_)) {
    if (node.get().active() && node.get().reads(key)) {
      dirty_.insert(node.get().id_);
      repaint(node.get());
    }
  }
  std::vector<std::reference_wrapper<Node>> transforming;
  for (const std::reference_wrapper<Node> node :
       std::views::values(registry_)) {
    if (node.get().active() && transforms(node.get(), key)) {
      transforming.push_back(node);
    }
  }
  std::ranges::sort(transforming, {},
                    [](const std::reference_wrapper<Node>& node) {
                      return node.get().depth_;
                    });
  for (const std::reference_wrapper<Node> node : transforming) {
    static_cast<void>(rematerialize(node.get(), key));
  }
  const auto current = motion::TransactionStack::current();
  if (current.has_value() && ambient(current->get())) {
    current->get().attach(completions_);
    candidate_ = current->get();
  }
}

auto Tree::layout_slot(const Node& node)
    -> std::optional<std::reference_wrapper<LayoutSlot>> {
  const auto found = lookup(node.id());
  if (!found.has_value() || found->get().layout_slot_ == nullptr) {
    return std::nullopt;
  }
  return std::ref(*found->get().layout_slot_);
}

auto Tree::attach_layout_slot(const Node& node,
                              std::unique_ptr<LayoutSlot> slot) -> LayoutSlot& {
  Node& owner = lookup(node.id())->get();
  owner.layout_slot_ = std::move(slot);
  return *owner.layout_slot_;
}

auto Tree::take_duplicate_keys() -> std::vector<DuplicateKey> {
  return std::exchange(duplicate_keys_, {});
}

auto Tree::take_failures() -> std::vector<FailureReport> {
  return std::exchange(reports_, {});
}

auto Tree::reset_boundary(NodeId id) -> bool {
  assert(owner_());
  const auto found = lookup(id);
  if (!found.has_value()) {
    return false;
  }
  Node& boundary = found->get();
  if (!boundary.boundary() || !boundary.active() ||
      !boundary.boundary_failure_.has_value()) {
    return false;
  }
  std::optional<NodeId> parent = boundary.parent_;
  while (parent.has_value()) {
    const auto ancestor = lookup(*parent);
    if (!ancestor.has_value()) {
      return false;
    }
    if (ancestor->get().content_->composes()) {
      boundary.boundary_failure_.reset();
      boundary.boundary_rebuild_ = false;
      dirty_.insert(ancestor->get().id_);
      return true;
    }
    parent = ancestor->get().parent_;
  }
  return false;
}

void Tree::cancel_tasks() noexcept {
  for (const std::reference_wrapper<Node> node :
       std::views::values(registry_)) {
    node.get().tasks_.clear();
  }
  task_nodes_.clear();
}

auto Tree::running_tasks() const noexcept -> std::size_t {
  std::size_t total = 0;
  for (const std::reference_wrapper<Node> node :
       std::views::values(registry_)) {
    total += node.get().running_tasks();
  }
  return total;
}

auto Tree::scheduler() const noexcept -> NodeTask::Scheduler {
  if (!services_.has_value()) {
    return std::nullopt;
  }
  return services_->frames;
}

auto Tree::allocate() noexcept -> NodeId { return NodeId{next_id_++}; }

void Tree::adopt(Node& node) {
  index(node);
  if (node.content_as<Layer::Content>().has_value()) {
    layers_.insert(node.id());
  }
  if (spare_entries_.empty()) {
    registry_.emplace(node.id(), node);
    return;
  }
  Registry::node_type entry = std::move(spare_entries_.back());
  spare_entries_.pop_back();
  entry.key() = node.id();
  entry.mapped() = node;
  registry_.insert(std::move(entry));
}

void Tree::dismantle(std::vector<std::unique_ptr<Node>>& children) {
  dismantle_.run(children);
}

void Tree::forget(NodeId id) noexcept {
  if (const auto found = registry_.find(id); found != registry_.end()) {
    unindex(found->second.get());
  }
  const auto pooled = recycled_.extract(id);
  if (Registry::node_type entry = registry_.extract(id);
      !entry.empty() && spare_entries_.size() < spare_entry_limit) {
    spare_entries_.push_back(std::move(entry));
  }
  dirty_.erase(id);
  deferred_.erase(id);
  layers_.erase(id);
  preference_readers_.erase(id);
  scroll_watchers_.erase(id);
  geometry_watchers_.erase(id);
  focus_readers_.erase(id);
  focus_within_readers_.erase(id);
  hover_readers_.erase(id);
  for (auto&& [key, readers] : focused_value_readers_) {
    readers.erase(id);
  }
  std::erase_if(focused_value_readers_,
                [](const auto& entry) { return entry.second.empty(); });
  focused_values_readers_.erase(id);
  transactions_.erase(id);
  std::erase_if(sources_,
                [id](const auto& entry) { return entry.second == id; });
}

auto Tree::exclusive_frame(ExclusiveFrame::Options options,
                           std::optional<NodeId> owner) -> ExclusiveFrame {
  assert(owner_());
  const std::uint64_t id = exclusive_->acquire(options, owner, focused_);
  return ExclusiveFrame{
      std::move(options),
      support::ScopedCallback{
          [scopes = std::weak_ptr<ExclusiveScopes>{exclusive_}, id]() noexcept {
            if (const std::shared_ptr<ExclusiveScopes> held = scopes.lock()) {
              held->release(id);
            }
          }}};
}

auto Tree::matched_source(const MatchedGeometry::Key& key) const
    -> std::optional<NodeId> {
  const auto found = sources_.find(key);
  if (found == sources_.end()) {
    return std::nullopt;
  }
  return found->second;
}

void Tree::present_debug_layer(std::optional<Element> content) {
  assert(owner_());
  if (!content.has_value() && !debug_layer_.has_value() &&
      !debug_content_.has_value()) {
    return;
  }
  if (content.has_value()) {
    std::vector<Element> children;
    children.push_back(std::move(*content));
    debug_content_.emplace(Layer(OverlayLayer::debug, OverlayInput::modal,
                                 Alignment::top_leading, std::move(children))
                               .transition(motion::Transition::identity()));
  } else {
    debug_content_.reset();
  }
  debug_changed_ = true;
  dirty_.insert(root_->id());
}

void Tree::present_overlay(std::uint64_t id, std::optional<Element> content,
                           OverlayEnvironment environment) {
  assert(owner_());
  overlay_requests_.push_back(OverlayRequest{
      .id = id, .content = std::move(content), .environment = environment});
  dirty_.insert(root_->id());
}

auto Tree::debugs(const Node& node) const -> bool {
  if (!debug_layer_.has_value()) {
    return false;
  }
  for (std::optional<NodeId> at = node.id(); at.has_value();) {
    if (*at == *debug_layer_) {
      return true;
    }
    const auto found = registry_.find(*at);
    if (found == registry_.end()) {
      return false;
    }
    const Node& current = found->second.get();
    if (current.depth() <= 1) {
      return false;
    }
    at = current.parent();
  }
  return false;
}

auto Tree::lookup(NodeId id) -> std::optional<std::reference_wrapper<Node>> {
  const auto found = registry_.find(id);
  if (found == registry_.end()) {
    return std::nullopt;
  }
  return found->second;
}

auto Tree::search(const Node& node, const ElementKey& key) const
    -> std::optional<NodeId> {
  if (!node.active()) {
    return std::nullopt;
  }
  if (node.identified_by(key)) {
    return node.id_;
  }
  for (const Node& child : node.children()) {
    if (const auto found = search(child, key)) {
      return found;
    }
  }
  return std::nullopt;
}

}
