#pragma once

#include <arisa/composition/axes.hpp>
#include <arisa/composition/axis.hpp>
#include <arisa/composition/layer.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/node_id.hpp>
#include <arisa/composition/point.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/composition/scroll_geometry.hpp>
#include <arisa/composition/scroll_phase.hpp>
#include <arisa/composition/scroll_phase_change_context.hpp>
#include <arisa/composition/scroll_position.hpp>
#include <arisa/composition/scroll_presentation.hpp>
#include <arisa/composition/scroll_request.hpp>
#include <arisa/composition/scrolling.hpp>
#include <arisa/composition/size.hpp>
#include <arisa/composition/tree.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/transaction.hpp>
#include <arisa/motion/unit_point.hpp>
#include <arisa/provider/input/stick.hpp>
#include <arisa/support/full_order_walk.hpp>

#include <interaction/invocation.hpp>
#include <interaction/scroller.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace arisa::interaction {

class ScrollCoordinator {
 public:
  static constexpr float wheel_step = 40.0F;
  static constexpr float stick_deadzone = 0.05F;
  static constexpr int request_frames = 4;
  static constexpr std::chrono::milliseconds stick_step_limit{50};

  static constexpr float page_share = 0.9F;
  static constexpr float movable_threshold = 0.5F;
  static constexpr float refresh_pull = 64.0F;

  enum class Page : std::uint8_t {
    up,
    down,
    start,
    end,
  };

  struct Report {
    bool changed{false};
    std::vector<Invocation> invocations;
  };

  static auto follow() -> motion::Animation;

  void synchronise(const composition::Tree& tree);

  void requests(composition::Tree& tree, std::chrono::nanoseconds now,
                Report& report);

  void step(composition::Tree& tree, std::chrono::nanoseconds now,
            Report& report);

  auto scrollable(composition::NodeId node,
                  composition::ScrollInputKind input =
                      composition::ScrollInputKind::touch) const
      -> std::optional<composition::Axes>;

  void grab(composition::NodeId node, std::chrono::nanoseconds now);

  void drag(composition::NodeId node, composition::Size translation,
            std::chrono::nanoseconds now);

  void release(composition::NodeId node, composition::Size velocity,
               std::chrono::nanoseconds now);

  auto reveal(const composition::Tree& tree, composition::NodeId focused,
              float margin, std::chrono::nanoseconds now) -> bool;

  auto stick(const composition::Tree& tree,
             std::optional<composition::NodeId> focused, provider::Stick right,
             float speed, std::chrono::nanoseconds now) -> bool;

  auto wheel(std::span<const composition::NodeId> chain, float x, float y,
             std::chrono::nanoseconds now) -> bool;

  auto page(composition::Tree& tree, std::optional<composition::NodeId> focused,
            Page kind, std::chrono::nanoseconds now) -> bool;

  auto scrollable_toward(const composition::Tree& tree,
                         composition::NodeId node,
                         composition::MoveCommandDirection direction) const
      -> std::optional<composition::NodeId>;

  auto nudge(composition::NodeId container,
             composition::MoveCommandDirection direction,
             const composition::Rect& keep, float margin,
             std::chrono::nanoseconds now) -> bool;

  auto at_start(composition::NodeId node, composition::Axis axis) const -> bool;

  auto overscrolled(composition::NodeId node) const -> bool;

  auto step_toward(composition::NodeId container,
                   composition::MoveCommandDirection direction,
                   std::chrono::nanoseconds now) -> bool;

  auto followed() const noexcept -> std::optional<composition::NodeId> {
    return followed_;
  }

  void unfollow() noexcept { followed_.reset(); }

  void set_horizontally_compact(bool compact) noexcept {
    if (compact != compact_) {
      compact_ = compact;
      stamp_.reset();
    }
  }

  auto scroller(composition::NodeId node) const
      -> std::optional<std::reference_wrapper<const Scroller>>;

  auto containers() const noexcept -> std::size_t { return containers_.size(); }

  auto synchronisations() const noexcept -> std::uint64_t {
    return synchronisations_;
  }

  auto take_keyboard_dismissal() noexcept -> bool {
    return std::exchange(dismissing_, false);
  }

 private:
  struct Container {
    Scroller scroller;
    composition::Rect viewport{};
    theme::Edges insets{};
    composition::Rect visible{};
    composition::Axes axes{composition::Axes::vertical};
    bool disabled{false};
    composition::ScrollDismissesKeyboardMode keyboard{
        composition::ScrollDismissesKeyboardMode::automatic};
    composition::Scrolling::Inputs inputs;
    composition::Size dragged{};
    composition::ScrollGeometry geometry{};
    composition::ScrollPhase phase{composition::ScrollPhase::idle};
    composition::ScrollPresentation published{};
    bool seen{false};
    std::shared_ptr<std::move_only_function<void()>> refresh;
    std::shared_ptr<composition::ScrollGeometryObserver> nested;
    float pulled{0.0F};
    std::vector<std::vector<composition::ElementKey>> reported;
    std::optional<std::optional<composition::ElementKey>> position;
    composition::Axes indicator_axes{composition::Axes::all};
    composition::Axes fitted{};
    composition::Scrolling::Anchors anchors;
    bool measured{false};
    bool anchored{false};
    composition::Size content_seen{};
    composition::Size viewport_seen{};
    std::optional<composition::ElementKey> flash_seen;
    bool flashing{false};
    std::shared_ptr<const composition::Binding<composition::ScrollPosition>>
        placement;
    std::optional<composition::ScrollPosition> placed;
    bool placing{false};
    bool touched{false};
    std::optional<motion::UnitPoint> placement_anchor;
    std::optional<composition::NodeId> steady_node;
    float steady_start{0.0F};
    composition::Point steady_offset{};
  };

  using Refresh = std::shared_ptr<std::move_only_function<void()>>;

  struct Inherited {
    std::optional<bool> disabled;
    std::optional<composition::ScrollTargetBehavior> behavior;
    std::optional<composition::ViewAlignedScrollTargetBehavior> aligned;
    std::optional<composition::AnyScrollTargetBehavior> custom;
    bool compact{true};
    std::optional<motion::UnitPoint> placement_anchor;
    composition::Scrolling::Along<composition::ScrollIndicatorVisibility>
        indicators;
    composition::Scrolling::Along<composition::ScrollBounceBehavior> bounce;
    std::optional<composition::ScrollDecelerationRate> rate;
    std::optional<composition::ScrollDismissesKeyboardMode> keyboard;
    composition::Scrolling::Inputs inputs;
    composition::Scrolling::Anchors anchors;
    std::optional<composition::Scrolling::Flash> flash;
    Refresh refresh;
    std::shared_ptr<const composition::Binding<composition::ScrollPosition>>
        placement;
    std::shared_ptr<composition::ScrollGeometryObserver> nested;
  };

  struct Watcher {
    composition::NodeId node{0};
    composition::NodeId container{0};
  };

  struct Lazy {
    composition::NodeId node{0};
    std::optional<composition::NodeId> container;
  };

  struct Pending {
    composition::ScrollRequest request;
    int frames{request_frames};
  };

  struct Reach {
    std::optional<composition::NodeId> container{};
    std::size_t below{0};
    std::size_t pushed{0};
  };

  struct NodeAccess {
    using Node = const composition::Node;

    static auto children(const composition::Node& node)
        -> decltype(std::declval<const composition::Node&>().children()) {
      return node.children();
    }
  };

  using Walk = support::FullOrderWalk<NodeAccess, Reach>;

  class Visitor {
   public:
    Visitor(ScrollCoordinator& coordinator,
            const composition::Tree& tree) noexcept
        : coordinator_(coordinator), tree_(tree) {}

    auto enter(const composition::Node& node, const Reach& above) -> Walk::Step;

    void leave(const composition::Node& node, Reach& reach);

   private:
    std::reference_wrapper<ScrollCoordinator> coordinator_;
    std::reference_wrapper<const composition::Tree> tree_;
  };

  auto visit(const composition::Tree& tree, const composition::Node& node,
             const Reach& above) -> Walk::Step;

  static auto inherit(const Inherited& outer,
                      const composition::Scrolling& scrolling) -> Inherited;

  static auto declares(const composition::Scrolling& scrolling) noexcept
      -> bool;

  static auto consumed(const Inherited& settings, composition::Axes axes)
      -> Inherited;

  static auto dismisses_on_scroll(const Container& container) noexcept -> bool;

  void adopt(const composition::Tree& tree, const composition::Node& node,
             Container& entry, const Inherited& settings, bool created,
             const motion::Transaction& transaction);

  void steady(const composition::Node& node, Container& entry,
              const motion::Transaction& transaction);

  void remember(const composition::Node& node, Container& entry);

  static auto start_of(const composition::Rect& frame, bool vertical) noexcept
      -> float;

  static auto bounces(std::optional<composition::ScrollBounceBehavior> behavior,
                      float content, float viewport) noexcept -> bool;

  static auto alignment_shift(const Container& container) -> composition::Point;

  void place(composition::Tree& tree, composition::NodeId id,
             Container& container, composition::ScrollPhase phase,
             std::chrono::nanoseconds now, Report& report);

  auto top_key(const composition::Node& node, const Container& container)
      -> std::optional<composition::ElementKey>;

  void rearrange(const composition::Tree& tree);

  void observe(composition::Tree& tree, composition::NodeId id,
               Container& container, composition::ScrollPhase before,
               composition::ScrollPhase after, Report& report);

  static auto visible_rect(const Container& container) -> composition::Rect;

  static auto visible_of(const composition::Node& node,
                         const composition::Rect& viewport)
      -> composition::Rect;

  static auto share(const composition::Rect& item,
                    const composition::Rect& visible, composition::Axes axes)
      -> double;

  auto items(const composition::Node& node)
      -> std::span<const std::reference_wrapper<const composition::Node>>;

  void retarget(const composition::Node& node, composition::Axes axes,
                const composition::Rect& viewport);

  static auto phase_context(const Container& container,
                            const composition::ScrollGeometry& geometry,
                            composition::ScrollPhase phase)
      -> composition::ScrollPhaseChangeContext;

  static void gather(
      const composition::Node& node,
      std::vector<std::reference_wrapper<const composition::Node>>& found);

  static auto marked(const composition::Node& node)
      -> std::optional<std::reference_wrapper<const composition::Node>>;

  auto choose(const composition::Tree& tree,
              std::optional<composition::NodeId> focused,
              composition::Axis axis) const
      -> std::optional<composition::NodeId>;

  auto enclosing(const composition::Tree& tree, composition::NodeId node) const
      -> std::vector<composition::NodeId>;

  template <typename Visit>
  void each_enclosing(const composition::Tree& tree, composition::NodeId node,
                      Visit&& visit) const {
    std::optional<composition::NodeId> current = node;
    bool first = true;
    while (current.has_value()) {
      const auto entry = tree.node(*current);
      if (!entry.has_value()) {
        break;
      }
      if (!first && containers_.contains(*current)) {
        visit(*current);
      }
      if (entry->get().content_as<composition::Layer::Content>().has_value()) {
        break;
      }
      first = false;
      current = entry->get().parent();
    }
  }

  auto desired(const Container& container, const composition::Rect& rect,
               std::optional<motion::UnitPoint> anchor, float margin) const
      -> composition::Point;

  auto bring(const composition::Tree& tree, composition::NodeId from,
             composition::Rect rect, std::optional<motion::UnitPoint> anchor,
             float margin, std::optional<motion::Animation> animation,
             std::chrono::nanoseconds now) -> bool;

  static auto geometry_of(const Container& container)
      -> composition::ScrollGeometry;

  auto within(const composition::Tree& tree, composition::NodeId node,
              composition::NodeId ancestor) const -> bool;

  auto row_stop(composition::Tree& tree, composition::NodeId container,
                const Container& entry, float wanted, float current,
                bool forward) const -> std::optional<float>;

  std::map<composition::NodeId, Container> containers_;
  std::vector<Lazy> lazies_;
  std::vector<Inherited> inherited_;
  Walk walk_;
  std::vector<Watcher> watchers_;
  std::map<composition::NodeId, bool> watched_;
  std::vector<Pending> pending_;
  std::optional<std::chrono::nanoseconds> stick_clock_;
  std::optional<composition::NodeId> followed_;
  std::optional<std::pair<std::uint64_t, std::uint64_t>> stamp_;
  std::uint64_t arranged_{0};
  std::uint64_t synchronisations_{0};
  std::vector<std::reference_wrapper<const composition::Node>> items_;
  Scroller::Metrics metrics_;
  bool compact_{true};
  bool dismissing_{false};
};

}
