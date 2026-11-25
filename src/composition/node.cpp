#include <arisa/composition/error_boundary.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/tree.hpp>

#include <composition/node_lifecycle.hpp>

#include <algorithm>
#include <utility>

namespace arisa::composition {

Node::Node(Tree& tree, NodeId id, std::unique_ptr<ElementContent> content,
           std::optional<ElementKey> key, std::optional<ElementKey> alias,
           std::optional<NodeId> parent, std::size_t depth)
    : tree_(tree),
      id_(id),
      content_(std::move(content)),
      key_(std::move(key)),
      alias_(std::move(alias)),
      parent_(parent),
      depth_(depth),
      lifecycle_(std::make_unique<Lifecycle>()),
      anchor_(std::make_shared<NodeAnchor>(tree, id)) {
  tree.adopt(*this);
}

Node::~Node() {
  tree_.get().forget(id_);
  if (support::Dismantle<Lineage>::nested(children_)) {
    tree_.get().dismantle(children_);
  }
}

auto Node::layout_direction() const noexcept -> LayoutDirection {
  return layout_slot_ != nullptr ? layout_slot_->layout_direction()
                                 : LayoutDirection::left_to_right;
}

auto Node::phase() const noexcept -> NodeLifecyclePhase {
  return lifecycle_->phase();
}

auto Node::active() const noexcept -> bool {
  const NodeLifecyclePhase current = phase();
  return current == NodeLifecyclePhase::mounting ||
         current == NodeLifecyclePhase::live;
}

auto Node::identified_by(const ElementKey& key) const noexcept -> bool {
  return key_ == key || alias_ == key;
}

auto Node::overrides(Identity key) const noexcept -> bool {
  return std::ranges::any_of(
      overrides_,
      [key](const EnvironmentOverride& entry) { return entry.key() == key; });
}

auto Node::reads(Identity key) const noexcept -> bool {
  return environment_reads_.contains(key) ||
         environment_reads_.contains(EnvironmentValues::everything());
}

auto Node::running_tasks() const noexcept -> std::size_t {
  return static_cast<std::size_t>(
      std::ranges::count_if(tasks_, &NodeTask::running));
}

auto Node::boundary() const noexcept -> bool {
  return content_->type() == ElementType::of<ErrorBoundary::Content>();
}

void Node::observe(ObservableCore& core) {
  const Identity identity{&core};
  if (dependencies_.contains(identity)) {
    return;
  }
  if (const auto stale = stale_dependencies_.find(identity);
      stale != stale_dependencies_.end() &&
      stale->second.source.lock().get() == &core) {
    dependencies_.emplace(identity, std::move(stale->second));
    stale_dependencies_.erase(stale);
    return;
  }
  anchor_lent_ = true;
  std::weak_ptr<const NodeAnchor> anchor = anchor_;
  dependencies_.emplace(
      identity,
      Dependency{.source = core.weak_from_this(),
                 .subscription = core.subscribe([anchor = std::move(anchor)] {
                   if (const auto alive = anchor.lock()) {
                     alive->mark_dirty();
                   }
                 })});
}

auto Node::passes_through() const noexcept -> bool {
  const Interactions& held = *interactions_;
  return content_->passes_through() && *modifiers_ == Modifiers{} &&
         overrides_.empty() && transforms_.empty() &&
         !motion_->transition.has_value() && !motion_->matched.has_value() &&
         held.hit_testing && !held.disabled && !held.focusable.has_value() &&
         held.gestures.empty() && held.commands.empty() &&
         held.key_presses.empty() && !held.focus_section &&
         !held.focus_scope.has_value() && held.effects.empty() &&
         !held.coordinate_space.has_value() && held.geometry.empty() &&
         held.drag == nullptr && held.drop == nullptr &&
         held.modifier_keys.empty() && !held.type_select.has_value() &&
         !held.type_select_cleared && !held.drag_configuration.has_value() &&
         held.drag_updates == nullptr && held.drop_updates == nullptr &&
         held.drop_configuration == nullptr &&
         !held.drag_previews.has_value() && !held.drop_previews.has_value() &&
         held.drag_container == nullptr && held.drag_selection == nullptr &&
         !held.spring_loading.has_value() && !held.reorder_item.has_value() &&
         !held.reorder_container;
}

auto Node::scrolled_frame() const -> Rect {
  return tree_.get().scrolled_frame(*this);
}

}
