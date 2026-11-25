#include <arisa/composition/context.hpp>
#include <arisa/composition/focus_environment.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/reduce_motion.hpp>
#include <arisa/composition/tree.hpp>

#include <algorithm>
#include <cstdio>
#include <exception>
#include <print>
#include <utility>

namespace arisa::composition {

auto Context::slot_for(Identity key, SlotFactory create)
    -> std::shared_ptr<StateSlot> {
  Node& node = node_;
  if (const auto found = node.state_.find(key); found != node.state_.end()) {
    return found->second;
  }
  auto slot = create();
  node.state_.emplace(key, slot);
  return slot;
}

auto Context::services() const -> const RootServices& {
  const std::optional<RootServices>& services = tree_.get().services_;
  if (!services.has_value()) {
    std::println(stderr, "arisa: this tree was built without root services");
    std::terminate();
  }
  return *services;
}

auto Context::namespace_id(const Namespace& space) const -> Namespace::ID {
  return space.id(node_.get().id_);
}

auto Context::frame_time() const -> std::chrono::nanoseconds {
  return tree_.get().frame_time_;
}

auto Context::transaction() const -> const motion::Transaction& {
  return tree_.get().transaction(node_);
}

auto Context::motion_moment() -> motion::Presentation::Moment {
  return motion::Presentation::Moment{
      .transaction = transaction(),
      .now = frame_time(),
      .reduce_motion = environment(ReduceMotion::key),
      .cross_fade = false,
      .environment = [this] { return environment_snapshot(); }};
}

auto Context::node_anchor() const -> std::weak_ptr<const NodeAnchor> {
  return anchor();
}

auto Context::scroll_view_proxy() const -> ScrollViewProxy {
  return ScrollViewProxy{tree_.get().scroll_requests_, node_.get().id_};
}

auto Context::is_focused() -> bool {
  Node& node = node_;
  node.highlight_reads_ = true;
  return tree_.get().focused_ == node.id_;
}

auto Context::focus_within() -> bool {
  Tree& tree = tree_;
  const Node& node = node_;
  tree.focus_within_readers_.insert(node.id_);
  return tree.contains_focus(node);
}

auto Context::hover_within() -> bool {
  Tree& tree = tree_;
  const Node& node = node_;
  tree.hover_readers_.insert(node.id_);
  return tree.contains_hover(node);
}

auto Context::focused_lookup(Identity key)
    -> const EnvironmentOverride::Value* {
  return tree_.get().focused_lookup(key, node_.get().id_);
}

auto Context::focused_values() -> FocusedValues {
  return FocusedValues{tree_.get().focused_snapshot(node_.get().id_)};
}

auto Context::is_pressed() -> bool {
  Node& node = node_;
  node.highlight_reads_ = true;
  return tree_.get().pressed_ == node.id_;
}

void Context::request_frame() { tree_.get().dirty_.insert(node_.get().id_); }

void Context::select_link(std::size_t link) {
  Node& node = node_.get();
  if (node.selected_link_ != link) {
    node.selected_link_ = link;
    tree_.get().repaint(node);
  }
}

void Context::fail(UiError error) {
  tree_.get().failures_.push_back(
      Tree::Failure{.node = node_.get().id_, .error = std::move(error)});
}

auto Context::anchor() const -> std::weak_ptr<const NodeAnchor> {
  Node& node = node_;
  node.anchor_lent_ = true;
  return node.anchor_;
}

void Context::observe(ObservableCore& core) { node_.get().observe(core); }

auto Context::focus_flag(const Node& reader)
    -> std::reference_wrapper<const EnvironmentOverride::Value> {
  Tree& tree = tree_;
  const std::optional<NodeId> owner = tree.focus_owner(reader);
  if (!owner.has_value()) {
    return std::cref(tree.focus_false_.value());
  }
  tree.focus_readers_[*owner].insert(reader.id_);
  return std::cref(tree.focused_ == *owner ? tree.focus_true_.value()
                                           : tree.focus_false_.value());
}

auto Context::lookup(Identity key)
    -> std::optional<std::reference_wrapper<const EnvironmentOverride::Value>> {
  Node& reader = node_;
  reader.environment_reads_.insert(key);
  if (key == FocusEnvironment::is_focused.identity()) {
    return focus_flag(reader);
  }
  std::optional<std::reference_wrapper<Node>> current{reader};
  while (current.has_value()) {
    const Node& node = current->get();
    for (const EnvironmentOverride& entry : node.overrides_) {
      if (entry.key() == key) {
        return std::cref(entry.value());
      }
    }
    if (!node.parent_.has_value()) {
      break;
    }
    current = tree_.get().lookup(*node.parent_);
  }
  return std::nullopt;
}

auto Context::environment_values() -> EnvironmentValues {
  Node& reader = node_;
  reader.environment_reads_.insert(EnvironmentValues::everything());
  return tree_.get().environment_values(reader);
}

auto Context::environment_snapshot() const -> EnvironmentValues {
  return tree_.get().environment_values(node_);
}

auto Context::lookup_all(Identity key)
    -> std::vector<std::reference_wrapper<const EnvironmentOverride::Value>> {
  Node& reader = node_;
  reader.environment_reads_.insert(key);
  std::vector<std::reference_wrapper<const EnvironmentOverride::Value>> found;
  std::optional<std::reference_wrapper<Node>> current{reader};
  while (current.has_value()) {
    const Node& node = current->get();
    for (const EnvironmentOverride& entry : node.overrides_) {
      if (entry.key() == key) {
        found.push_back(std::cref(entry.value()));
      }
    }
    if (!node.parent_.has_value()) {
      break;
    }
    current = tree_.get().lookup(*node.parent_);
  }
  return found;
}

}
