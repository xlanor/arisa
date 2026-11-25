#pragma once

#include <arisa/composition/change_observer.hpp>
#include <arisa/composition/component_content.hpp>
#include <arisa/composition/container_corners.hpp>
#include <arisa/composition/departure_hold.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/environment_override.hpp>
#include <arisa/composition/environment_transform.hpp>
#include <arisa/composition/environment_values.hpp>
#include <arisa/composition/exclusive_frame.hpp>
#include <arisa/composition/geometry_proxy.hpp>
#include <arisa/composition/identity.hpp>
#include <arisa/composition/implicit_animation.hpp>
#include <arisa/composition/interactions.hpp>
#include <arisa/composition/keyed_values.hpp>
#include <arisa/composition/layout_slot.hpp>
#include <arisa/composition/link_frame.hpp>
#include <arisa/composition/matched_geometry.hpp>
#include <arisa/composition/modifiers.hpp>
#include <arisa/composition/motion_declarations.hpp>
#include <arisa/composition/node_anchor.hpp>
#include <arisa/composition/node_cost.hpp>
#include <arisa/composition/node_hooks.hpp>
#include <arisa/composition/node_id.hpp>
#include <arisa/composition/node_lifecycle_phase.hpp>
#include <arisa/composition/node_task.hpp>
#include <arisa/composition/observable_core.hpp>
#include <arisa/composition/preference.hpp>
#include <arisa/composition/preference_key.hpp>
#include <arisa/composition/presented_frame.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/composition/scroll_presentation.hpp>
#include <arisa/composition/size.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/state_slot.hpp>
#include <arisa/composition/ui_error.hpp>
#include <arisa/motion/presentation.hpp>
#include <arisa/motion/transaction.hpp>
#include <arisa/motion/transition.hpp>
#include <arisa/support/dismantle.hpp>
#include <arisa/support/sparse.hpp>
#include <arisa/theme/space.hpp>

#include <cstddef>
#include <cstdint>
#include <flat_map>
#include <flat_set>
#include <functional>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <vector>

namespace arisa::composition {

class PreferenceObserver;
class Tree;

template <class... Policies>
class BasicNodeLifecycle;

class Node {
 public:
  Node(Tree& tree, NodeId id, std::unique_ptr<ElementContent> content,
       std::optional<ElementKey> key, std::optional<ElementKey> alias,
       std::optional<NodeId> parent, std::size_t depth);

  Node(const Node&) = delete;
  auto operator=(const Node&) -> Node& = delete;
  Node(Node&&) = delete;
  auto operator=(Node&&) -> Node& = delete;
  ~Node();

  auto id() const noexcept -> NodeId { return id_; }

  auto type() const noexcept -> ElementType { return content_->type(); }

  auto kind() const noexcept -> ElementType { return content_->kind(); }

  auto passes_through() const noexcept -> bool;

  auto key() const noexcept -> const std::optional<ElementKey>& { return key_; }

  auto parent() const noexcept -> std::optional<NodeId> { return parent_; }

  auto depth() const noexcept -> std::size_t { return depth_; }

  auto phase() const noexcept -> NodeLifecyclePhase;

  auto content() const noexcept -> const ElementContent& { return *content_; }

  template <class Content>
  auto content_as() const noexcept
      -> std::optional<std::reference_wrapper<const Content>> {
    if (content_->type() != ElementType::of<Content>()) {
      return std::nullopt;
    }
    return std::cref(static_cast<const Content&>(*content_));
  }

  template <class T>
  auto component_as() const noexcept
      -> std::optional<std::reference_wrapper<const T>> {
    const auto content = content_as<ComponentContent<T>>();
    if (!content.has_value()) {
      return std::nullopt;
    }
    return std::cref(content->get().component());
  }

  auto children() const {
    return children_ |
           std::views::transform(
               [](const std::unique_ptr<Node>& child) -> const Node& {
                 return *child;
               });
  }

  auto child_count() const noexcept -> std::size_t { return children_.size(); }

  auto dependencies() const noexcept -> std::size_t {
    return dependencies_.size();
  }

  auto state_slots() const noexcept -> std::size_t { return state_.size(); }

  template <class T>
  auto stored(const StateKey<T>& key) const
      -> std::optional<std::reference_wrapper<const T>> {
    const auto found = state_.find(key.identity());
    if (found == state_.end()) {
      return std::nullopt;
    }
    return std::cref(
        static_cast<const StateSlot::Typed<T>&>(*found->second).value());
  }

  auto running_tasks() const noexcept -> std::size_t;

  auto boundary_failure() const noexcept -> const std::optional<UiError>& {
    return boundary_failure_;
  }

  auto modifiers() const noexcept -> const Modifiers& { return *modifiers_; }

  auto container_values() const noexcept -> const KeyedValues& {
    return container_values_;
  }

  auto frame() const noexcept -> const Rect& { return frame_; }

  auto layout_direction() const noexcept -> LayoutDirection;

  auto interactions() const noexcept -> const Interactions& {
    return *interactions_;
  }

  auto presented() const noexcept -> const std::optional<PresentedFrame>& {
    return *presented_;
  }

  auto scroll_presentation() const noexcept -> const ScrollPresentation& {
    return scroll_presentation_;
  }

  auto link_frames() const noexcept -> std::span<const LinkFrame> {
    return link_frames_;
  }

  auto selected_link() const noexcept -> std::size_t { return selected_link_; }

  auto content_size() const noexcept -> const std::optional<Size>& {
    return content_size_;
  }

  auto content_frame() const noexcept -> const std::optional<Rect>& {
    return content_frame_;
  }

  auto safe_area() const noexcept -> const theme::Edges& { return safe_area_; }

  auto enclosure() const noexcept
      -> const std::optional<ContainerCorners::Enclosure>& {
    return enclosure_;
  }

  auto content_insets() const noexcept -> const theme::Edges& {
    return content_insets_;
  }

  auto transition() const noexcept -> const std::optional<motion::Transition>& {
    return motion_->transition;
  }

  auto ghost() const noexcept -> bool { return ghost_; }

  auto presentation() const noexcept -> const motion::Presentation& {
    return *presentation_;
  }

  auto matched_geometry() const noexcept
      -> const std::optional<MatchedGeometry>& {
    return motion_->matched;
  }

  auto zoom_source() const noexcept
      -> const std::optional<MatchedGeometry::Key>& {
    return motion_->zoom;
  }

  auto paint_stamp() const noexcept -> std::uint64_t { return paint_stamp_; }

  auto own_paint_stamp() const noexcept -> std::uint64_t {
    return own_paint_stamp_;
  }

  auto paint_slot() const noexcept -> std::uint32_t { return paint_slot_; }

  auto cost() const noexcept -> const NodeCost& { return cost_; }

  template <class T>
  auto preference(const PreferenceKey<T>& key) const -> std::optional<T> {
    std::optional<T> value;
    for (const std::unique_ptr<Node>& child : children_) {
      if (!child->active() || !child->preferring_ ||
          child->content_->isolates(key.identity())) {
        continue;
      }
      std::optional<T> found = child->preference(key);
      if (!found.has_value()) {
        continue;
      }
      if (value.has_value()) {
        key.reduce(*value, std::move(*found));
      } else {
        value = std::move(found);
      }
    }
    if (published_.has_value() && published_->key() == key.identity()) {
      value = published_->applied<T>(
          std::move(value), published_->anchored() ? scrolled_frame() : frame_);
    }
    for (const Preference& entry : preferences_) {
      if (entry.key() == key.identity()) {
        value = entry.applied<T>(std::move(value),
                                 entry.anchored() ? scrolled_frame() : frame_);
      }
    }
    return value;
  }

  template <class T>
  auto override_of(const EnvironmentKey<T>& key) const noexcept
      -> std::optional<std::reference_wrapper<const T>> {
    for (const EnvironmentOverride& entry : overrides_) {
      if (entry.key() == key.identity()) {
        return std::cref(
            static_cast<const EnvironmentOverride::Typed<T>&>(entry.value())
                .get());
      }
    }
    return std::nullopt;
  }

 private:
  friend class Context;
  friend class Reconciler;
  friend class Tree;

  struct Lineage {
    using Owned = std::unique_ptr<Node>;

    static auto children(Owned& node) noexcept
        -> std::vector<std::unique_ptr<Node>>& {
      return node->children_;
    }
  };

  struct Dependency {
    std::weak_ptr<ObservableCore> source;
    Subscription subscription;
  };

  using Lifecycle = BasicNodeLifecycle<>;

  auto lifecycle() noexcept -> Lifecycle& { return *lifecycle_; }

  auto active() const noexcept -> bool;

  auto overrides(Identity key) const noexcept -> bool;

  auto reads(Identity key) const noexcept -> bool;

  auto identified_by(const ElementKey& key) const noexcept -> bool;

  auto boundary() const noexcept -> bool;

  auto scrolled_frame() const -> Rect;

  void observe(ObservableCore& core);

  std::reference_wrapper<Tree> tree_;
  NodeId id_;
  std::unique_ptr<ElementContent> content_;
  std::optional<ElementKey> key_;
  std::optional<ElementKey> alias_;
  std::optional<NodeId> parent_;
  std::size_t depth_;
  std::unique_ptr<Lifecycle> lifecycle_;
  std::shared_ptr<NodeAnchor> anchor_;
  bool anchor_lent_{false};
  std::flat_map<Identity, std::shared_ptr<StateSlot>> state_;
  std::vector<EnvironmentOverride> overrides_;
  std::vector<EnvironmentTransform> transforms_;
  std::vector<Preference> preferences_;
  std::optional<Preference> published_;
  std::vector<std::shared_ptr<PreferenceObserver>> preference_observers_;
  bool preferring_{false};
  bool anchored_{false};
  std::uint64_t preference_stamp_{0};
  std::uint64_t preference_resolved_{0};
  std::flat_set<Identity> environment_reads_;
  bool highlight_reads_{false};
  std::flat_map<Identity, Dependency> dependencies_;
  std::flat_map<Identity, Dependency> stale_dependencies_;
  NodeHooks hooks_;
  std::weak_ptr<const DepartureHold::Token> departure_;
  std::optional<UiError> boundary_failure_;
  bool boundary_rebuild_{false};
  std::vector<std::unique_ptr<Node>> children_;
  std::vector<NodeTask> tasks_;
  std::optional<ExclusiveFrame> exclusive_;
  support::Sparse<Modifiers> modifiers_;
  KeyedValues container_values_;
  support::Sparse<Interactions> interactions_;
  std::shared_ptr<std::vector<GeometryProxy::NamedSpace>> spaces_;
  Rect frame_;
  support::Sparse<std::optional<PresentedFrame>> presented_;
  ScrollPresentation scroll_presentation_;
  std::optional<Size> content_size_;
  std::vector<LinkFrame> link_frames_;
  std::size_t selected_link_{0};
  std::optional<Rect> content_frame_;
  theme::Edges safe_area_;
  std::optional<ContainerCorners::Enclosure> enclosure_;
  theme::Edges content_insets_;
  std::unique_ptr<LayoutSlot> layout_slot_;
  support::Sparse<MotionDeclarations> motion_;
  std::vector<std::unique_ptr<ChangeObserver>> changes_;
  std::optional<motion::Transaction> transaction_;
  support::Sparse<motion::Presentation> presentation_;
  std::optional<DepartureHold> transition_hold_;
  bool ghost_{false};
  bool fresh_{false};
  std::uint64_t paint_stamp_{0};
  std::uint64_t own_paint_stamp_{0};
  std::uint32_t paint_slot_{0};
  NodeCost cost_;
};

}
