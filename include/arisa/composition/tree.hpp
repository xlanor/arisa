#pragma once

#include <arisa/composition/duplicate_key.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/environment_override.hpp>
#include <arisa/composition/environment_transform.hpp>
#include <arisa/composition/environment_values.hpp>
#include <arisa/composition/exclusive_frame.hpp>
#include <arisa/composition/exclusive_scopes.hpp>
#include <arisa/composition/failure_report.hpp>
#include <arisa/composition/feedback_origin.hpp>
#include <arisa/composition/feedback_request.hpp>
#include <arisa/composition/focus_environment.hpp>
#include <arisa/composition/focus_resets.hpp>
#include <arisa/composition/geometry_proxy.hpp>
#include <arisa/composition/identity.hpp>
#include <arisa/composition/key_press.hpp>
#include <arisa/composition/layout_slot.hpp>
#include <arisa/composition/link_frame.hpp>
#include <arisa/composition/matched_geometry.hpp>
#include <arisa/composition/move_command_direction.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/node_id.hpp>
#include <arisa/composition/presented_frame.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/composition/reserved_region.hpp>
#include <arisa/composition/root_services.hpp>
#include <arisa/composition/scroll_presentation.hpp>
#include <arisa/composition/scroll_requests.hpp>
#include <arisa/composition/sensory_feedback.hpp>
#include <arisa/composition/size.hpp>
#include <arisa/composition/ui_error.hpp>
#include <arisa/motion/completion_queue.hpp>
#include <arisa/motion/presentation.hpp>
#include <arisa/motion/transaction.hpp>
#include <arisa/support/dismantle.hpp>
#include <arisa/support/scoped_callback.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <flat_map>
#include <flat_set>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace arisa::composition {

class PreferenceObserver;
class Reconciler;

class Tree {
 public:
  using OwnerCheck = std::move_only_function<bool() const>;

  static constexpr std::size_t preference_rounds = 4;

  enum class OverlayEnvironment : std::uint8_t {
    own,
    inherited,
  };

  Tree(Element content, OwnerCheck owner,
       std::optional<RootServices> services = std::nullopt);

  Tree(const Tree&) = delete;
  auto operator=(const Tree&) -> Tree& = delete;
  Tree(Tree&&) = delete;
  auto operator=(Tree&&) -> Tree& = delete;
  ~Tree();

  void reconcile();

  void settle_geometry();

  void mark_dirty(NodeId node);

  auto root() const noexcept -> const Node& { return *root_; }

  auto node(NodeId id) const
      -> std::optional<std::reference_wrapper<const Node>>;

  auto find(const ElementKey& key) const -> std::optional<NodeId>;

  auto find(const ElementKey& key, NodeId within) const
      -> std::optional<NodeId>;

  auto activate(NodeId id) -> bool;

  auto activate(NodeId id, std::optional<Point> at) -> bool;

  auto steer(NodeId id, MoveCommandDirection direction) -> bool;

  auto press(NodeId id, const KeyPress& press) -> bool;

  void enter(NodeId id, MoveCommandDirection direction);

  void assign_frame(NodeId id, Rect frame);

  auto scrolled_frame(const Node& node) const -> Rect;

  auto environment_values(const Node& node) const -> EnvironmentValues;

  auto defer_geometry() -> support::ScopedCallback;

  auto geometry_dirty() const -> bool;

  auto preference_readers() const noexcept -> std::size_t {
    return preference_readers_.size();
  }

  auto preference_resolutions() const noexcept -> std::uint64_t {
    return preference_resolutions_;
  }

  void highlight(std::optional<NodeId> focused, std::optional<NodeId> pressed);

  void hover(std::optional<NodeId> hovered);

  auto hovered() const noexcept -> std::optional<NodeId> { return hovered_; }

  auto focus_resets() const noexcept -> const std::shared_ptr<FocusResets>& {
    return focus_resets_;
  }

  auto focused_value_readers() const noexcept -> std::size_t {
    return focused_value_readers_.size();
  }

  void present(NodeId id, PresentedFrame frame);

  auto materialize(NodeId id, MoveCommandDirection direction) -> bool;

  auto reveal(NodeId id, const Rect& visible) -> bool;

  auto locate(NodeId id, const ElementKey& key) -> std::optional<Rect>;

  auto page_stop(NodeId id, float position) -> std::optional<float>;

  void present_scroll(NodeId id, ScrollPresentation presentation);

  void repaint(NodeId id);

  void seal_paint() noexcept { ++paint_epoch_; }

  auto paint_epoch() const noexcept -> std::uint64_t { return paint_epoch_; }

  void assign_paint_slot(NodeId id, std::uint32_t slot);

  void assign_content_size(NodeId id, std::optional<Size> size);

  void assign_link_frames(NodeId id, std::span<const LinkFrame> frames);

  void assign_published(NodeId id, std::optional<Preference> published);

  void assign_content_frame(NodeId id, std::optional<Rect> frame);

  void assign_safe_area(NodeId id, theme::Edges insets);

  void assign_enclosure(NodeId id,
                        std::optional<ContainerCorners::Enclosure> enclosure);

  auto container_corners(const Node& node) const
      -> std::optional<ContainerCorners>;

  void assign_content_insets(NodeId id, theme::Edges insets);

  void declare_reserved_regions(std::vector<ReservedRegion> regions);

  auto reserved_regions() const noexcept -> const GeometryProxy::Regions& {
    return reserved_regions_;
  }

  auto scroll_requests() const noexcept
      -> const std::shared_ptr<ScrollRequests>& {
    return scroll_requests_;
  }

  template <class T>
  void set_environment(const EnvironmentKey<T>& key,
                       const std::type_identity_t<T>& value) {
    if (publishes(key, value)) {
      return;
    }
    publish(EnvironmentOverride::of(key, T{value}));
  }

  template <class T>
  void set_environment(const EnvironmentKey<T>& key,
                       std::type_identity_t<T>&& value) {
    if (publishes(key, value)) {
      return;
    }
    publish(EnvironmentOverride::of(key, std::move(value)));
  }

  void request_feedback(NodeId node, SensoryFeedback feedback,
                        FeedbackOrigin origin);

  auto take_feedback() -> std::vector<SensoryFeedback>;

  auto layout_slot(const Node& node)
      -> std::optional<std::reference_wrapper<LayoutSlot>>;

  auto attach_layout_slot(const Node& node, std::unique_ptr<LayoutSlot> slot)
      -> LayoutSlot&;

  auto revision() const noexcept -> std::uint64_t { return revision_; }

  auto geometry() const noexcept -> std::uint64_t { return geometry_; }

  auto arrangement() const noexcept -> std::uint64_t { return arrangement_; }

  auto visibility() const noexcept -> std::uint64_t { return visibility_; }

  auto layouts() const noexcept -> std::uint64_t { return layouts_; }

  auto geometry_loops(std::size_t frames) -> std::vector<NodeId>;

  void relaid() noexcept {
    ++geometry_;
    ++arrangement_;
    ++layouts_;
  }

  auto dirty() const noexcept -> bool { return !dirty_.empty(); }

  auto recycles() const noexcept -> std::uint64_t { return recycles_; }

  auto recyclable() const noexcept -> std::size_t;

  auto departing() const noexcept -> std::size_t {
    return departing_roots_.size();
  }

  auto size() const noexcept -> std::size_t { return registry_.size(); }

  auto searches() const noexcept -> std::uint64_t { return searches_; }

  auto take_duplicate_keys() -> std::vector<DuplicateKey>;

  auto take_failures() -> std::vector<FailureReport>;

  auto reset_boundary(NodeId id) -> bool;

  void cancel_tasks() noexcept;

  auto running_tasks() const noexcept -> std::size_t;

  auto services() const noexcept -> const std::optional<RootServices>& {
    return services_;
  }

  auto on_owner_thread() const -> bool { return owner_(); }

  auto presentation(const Node& node) -> motion::Presentation&;

  auto transaction(const Node& node) const noexcept
      -> const motion::Transaction&;

  void close_transactions() noexcept;

  auto examined_transactions() const noexcept -> std::uint64_t {
    return examined_transactions_;
  }

  auto run_completions() -> std::size_t;

  auto completions_pending() const noexcept -> bool;

  auto evaluations() const noexcept -> std::uint64_t { return evaluations_; }

  auto created() const noexcept -> std::uint64_t { return next_id_; }

  auto transitions_held() const noexcept -> std::size_t {
    return transition_holds_.size();
  }

  auto matched_source(const MatchedGeometry::Key& key) const
      -> std::optional<NodeId>;

  void set_frame_time(std::chrono::nanoseconds now) noexcept {
    frame_time_ = now;
  }

  auto frame_time() const noexcept -> std::chrono::nanoseconds {
    return frame_time_;
  }

  auto exclusive_frame(ExclusiveFrame::Options options,
                       std::optional<NodeId> owner = std::nullopt)
      -> ExclusiveFrame;

  auto exclusive() const noexcept -> const ExclusiveScopes& {
    return *exclusive_;
  }

  auto exclusive() noexcept -> ExclusiveScopes& { return *exclusive_; }

  auto layers() const noexcept -> const std::flat_set<NodeId>& {
    return layers_;
  }

  auto focused() const noexcept -> std::optional<NodeId> { return focused_; }

  void present_debug_layer(std::optional<Element> content);

  void present_overlay(
      std::uint64_t id, std::optional<Element> content,
      OverlayEnvironment environment = OverlayEnvironment::own);

  auto overlays() const noexcept -> std::size_t {
    return overlay_nodes_.size();
  }

  auto debug_layer() const noexcept -> std::optional<NodeId> {
    return debug_layer_;
  }

  auto debug_evaluations() const noexcept -> std::uint64_t {
    return debug_evaluations_;
  }

  auto debugs(const Node& node) const -> bool;

 private:
  friend class Context;
  friend class Node;
  friend class Reconciler;

  class RootContent;

  static auto ambient(const motion::Transaction& transaction) noexcept -> bool;

  struct Failure {
    NodeId node{0};
    UiError error{""};
  };

  struct OverlayRequest {
    std::uint64_t id;
    std::optional<Element> content;
    OverlayEnvironment environment;
  };

  struct NodeIdHash {
    auto operator()(NodeId id) const noexcept -> std::size_t {
      return std::hash<std::uint64_t>{}(id.value());
    }
  };

  auto allocate() noexcept -> NodeId;

  void repaint(Node& node);

  static auto observes_geometry(const Node& node) noexcept -> bool;

  static auto projected(const Node& above, Rect frame) -> Rect;

  void notify_geometry(Node& node, bool scrolled = false);

  void follow_scroll(NodeId scroller);

  void reproject(NodeId projector);

  void renotify(NodeId ancestor, bool scrolled);

  void flush_geometry();

  void touch_preferences(Node& node);

  void enroll(const Node& node);

  auto resolve_preferences() -> bool;

  void settle_preferences();

  auto inherited(const Node& node, Identity key) const
      -> const EnvironmentOverride::Value*;

  auto transformed(const Node& node,
                   std::span<const EnvironmentTransform> transforms) const
      -> std::vector<EnvironmentOverride>;

  auto rematerialize(Node& node, Identity key) -> bool;

  static auto transforms(const Node& node, Identity key) noexcept -> bool;

  auto focus_owner(const Node& node) const -> std::optional<NodeId>;

  auto contains_focus(const Node& node) const -> bool;

  auto contains_hover(const Node& node) const -> bool;

  void resolve_focused_values();

  void collect_scene_values(
      const Node& node,
      std::flat_map<Identity, std::shared_ptr<const EnvironmentOverride>>&
          found) const;

  auto focused_snapshot(NodeId reader)
      -> std::flat_map<Identity, std::shared_ptr<const EnvironmentOverride>>;

  auto focused_lookup(Identity key, NodeId reader)
      -> const EnvironmentOverride::Value*;

  void release_focus_readers(std::optional<NodeId> owner);

  auto named_spaces(Node& node) const -> GeometryProxy::Spaces;

  auto scroll_viewport(const Node& node) const -> std::optional<Rect>;

  auto scroll_viewports(const Node& node) const -> GeometryProxy::ScrollViews;

  auto inside(const Node& node, NodeId ancestor) const -> bool;

  static auto arranges(const Node& node) noexcept -> bool;

  void repaint_subtree(Node& node);

  void publish(EnvironmentOverride entry);

  void adopt(Node& node);

  void label(Node& node, std::optional<ElementKey> key,
             std::optional<ElementKey> alias);

  void index(const Node& node);

  void index(std::size_t hash, NodeId id);

  void unindex(const Node& node) noexcept;

  void unindex(std::size_t hash, NodeId id) noexcept;

  auto reachable(const Node& node, NodeId top) const -> bool;

  auto indexed(const ElementKey& key, NodeId top) const
      -> std::optional<NodeId>;

  void forget(NodeId id) noexcept;

  void dismantle(std::vector<std::unique_ptr<Node>>& children);

  auto lookup(NodeId id) -> std::optional<std::reference_wrapper<Node>>;

  auto admits(const FeedbackRequest& request) const -> bool;

  auto search(const Node& node, const ElementKey& key) const
      -> std::optional<NodeId>;

  auto scheduler() const noexcept -> NodeTask::Scheduler;

  OwnerCheck owner_;
  support::Dismantle<Node::Lineage> dismantle_;
  std::optional<RootServices> services_;
  GeometryProxy::Regions reserved_regions_;
  std::uint64_t next_id_{0};
  std::uint64_t revision_{0};
  std::uint64_t geometry_{0};
  std::uint64_t arrangement_{0};
  std::uint64_t visibility_{0};
  std::uint64_t layouts_{0};
  using Registry =
      std::unordered_map<NodeId, std::reference_wrapper<Node>, NodeIdHash>;

  static constexpr std::size_t spare_entry_limit = 256;

  using KeyIndex = std::unordered_multimap<std::size_t, NodeId>;

  Registry registry_;
  std::vector<Registry::node_type> spare_entries_;
  KeyIndex keyed_;
  std::vector<KeyIndex::node_type> spare_keys_;
  mutable std::uint64_t searches_{0};
  std::flat_set<NodeId> dirty_;
  std::set<NodeId> geometry_dirtied_;
  std::map<NodeId, std::size_t> geometry_streaks_;
  std::vector<NodeId> placed_;
  std::size_t deferring_{0};
  bool releasing_{false};
  std::flat_set<NodeId> deferred_;
  std::flat_set<NodeId> preference_readers_;
  std::flat_set<NodeId> scroll_watchers_;
  std::flat_set<NodeId> geometry_watchers_;
  std::uint64_t preference_epoch_{1};
  std::uint64_t preference_resolutions_{0};
  bool anchors_moved_{false};
  std::vector<std::shared_ptr<PreferenceObserver>> delivering_;
  std::flat_map<NodeId, std::flat_set<NodeId>> focus_readers_;
  std::flat_set<NodeId> focus_within_readers_;
  std::optional<NodeId> hovered_;
  std::flat_set<NodeId> hover_readers_;
  std::shared_ptr<FocusResets> focus_resets_{std::make_shared<FocusResets>()};
  std::flat_map<Identity, std::shared_ptr<const EnvironmentOverride>>
      focused_values_;
  std::flat_map<Identity, std::flat_set<NodeId>> focused_value_readers_;
  std::flat_set<NodeId> focused_values_readers_;
  std::optional<NodeId> resolved_focus_;
  std::uint64_t resolved_revision_{0};
  bool resolved_once_{false};
  EnvironmentOverride focus_true_{
      EnvironmentOverride::of(FocusEnvironment::is_focused, true)};
  EnvironmentOverride focus_false_{
      EnvironmentOverride::of(FocusEnvironment::is_focused, false)};
  std::vector<NodeId> departing_roots_;
  std::vector<DuplicateKey> duplicate_keys_;
  std::set<NodeId> task_nodes_;
  std::vector<Failure> failures_;
  std::vector<FailureReport> reports_;
  std::vector<FeedbackRequest> feedback_;
  std::shared_ptr<motion::CompletionQueue> completions_{
      std::make_shared<motion::CompletionQueue>()};
  std::shared_ptr<ScrollRequests> scroll_requests_{
      std::make_shared<ScrollRequests>()};
  std::optional<Element> pending_;
  std::map<NodeId, motion::Transaction> transactions_;
  std::optional<motion::Transaction> candidate_;
  std::optional<motion::Transaction> pass_;
  std::flat_set<NodeId> touched_;
  std::flat_set<NodeId> held_;
  std::uint64_t held_epoch_{0};
  std::uint64_t examined_transactions_{0};
  std::vector<NodeId> transition_holds_;
  motion::Transaction none_;
  std::map<MatchedGeometry::Key, NodeId> sources_;
  std::chrono::nanoseconds frame_time_{0};
  std::optional<NodeId> focused_;
  std::shared_ptr<ExclusiveScopes> exclusive_{
      std::make_shared<ExclusiveScopes>()};
  std::flat_set<NodeId> layers_;
  std::optional<NodeId> pressed_;
  std::uint64_t evaluations_{0};
  std::uint64_t debug_evaluations_{0};
  std::optional<Element> debug_content_;
  std::optional<NodeId> debug_layer_;
  bool debug_changed_{false};
  std::vector<OverlayRequest> overlay_requests_;
  std::flat_map<std::uint64_t, NodeId> overlay_nodes_;
  std::uint64_t paint_epoch_{1};
  template <class T>
  auto publishes(const EnvironmentKey<T>& key, const T& value) const -> bool {
    const std::vector<EnvironmentOverride>& overrides = root_->overrides_;
    const auto existing =
        std::ranges::find(overrides, key.identity(), &EnvironmentOverride::key);
    return existing != overrides.end() &&
           static_cast<const EnvironmentOverride::Typed<T>&>(existing->value())
                   .get() == value;
  }

  std::unique_ptr<Node> root_;
  std::map<NodeId, std::vector<std::unique_ptr<Node>>> recycled_;
  std::unique_ptr<Reconciler> reconciler_;
  std::uint64_t recycles_{0};
};

}
