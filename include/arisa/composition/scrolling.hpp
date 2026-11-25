#pragma once

#include <arisa/composition/alignment.hpp>
#include <arisa/composition/any_scroll_target_behavior.hpp>
#include <arisa/composition/axes.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/safe_area_regions.hpp>
#include <arisa/composition/scroll_bounce_behavior.hpp>
#include <arisa/composition/scroll_deceleration_rate.hpp>
#include <arisa/composition/scroll_dismisses_keyboard_mode.hpp>
#include <arisa/composition/scroll_edge_effect_style.hpp>
#include <arisa/composition/scroll_geometry_observer.hpp>
#include <arisa/composition/scroll_indicator_visibility.hpp>
#include <arisa/composition/scroll_input_behavior.hpp>
#include <arisa/composition/scroll_input_kind.hpp>
#include <arisa/composition/scroll_phase.hpp>
#include <arisa/composition/scroll_phase_change_context.hpp>
#include <arisa/composition/scroll_position.hpp>
#include <arisa/composition/scroll_position_binding.hpp>
#include <arisa/composition/scroll_target_behavior.hpp>
#include <arisa/composition/scroll_target_visibility_observer.hpp>
#include <arisa/composition/scroll_visibility_observer.hpp>
#include <arisa/composition/view_aligned_scroll_target_behavior.hpp>
#include <arisa/motion/unit_point.hpp>
#include <arisa/theme/space.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace arisa::composition {

struct Scrolling {
  using PhaseAction = std::move_only_function<void(ScrollPhase, ScrollPhase)>;
  using PhaseContextAction = std::move_only_function<void(
      ScrollPhase, ScrollPhase, const ScrollPhaseChangeContext&)>;

  struct Container {
    Axes axes{Axes::vertical};
    bool shows_indicators{true};

    friend constexpr auto operator==(const Container&, const Container&)
        -> bool = default;
  };

  template <class T>
  struct Along {
    std::optional<T> horizontal;
    std::optional<T> vertical;

    void set(Axes axes, T value) {
      if (axes.contains(Axis::horizontal)) {
        horizontal = value;
      }
      if (axes.contains(Axis::vertical)) {
        vertical = value;
      }
    }

    auto over(const Along& outer) const -> Along {
      return Along{
          .horizontal = horizontal.has_value() ? horizontal : outer.horizontal,
          .vertical = vertical.has_value() ? vertical : outer.vertical};
    }

    auto empty() const noexcept -> bool {
      return !horizontal.has_value() && !vertical.has_value();
    }

    friend auto operator==(const Along&, const Along&) -> bool = default;
  };

  struct Inputs {
    std::optional<ScrollInputBehavior> touch;
    std::optional<ScrollInputBehavior> controller;
    std::optional<ScrollInputBehavior> pointer;

    void set(ScrollInputKind kind, ScrollInputBehavior chosen) {
      switch (kind) {
        case ScrollInputKind::touch:
          touch = chosen;
          break;
        case ScrollInputKind::controller:
          controller = chosen;
          break;
        case ScrollInputKind::pointer:
          pointer = chosen;
          break;
      }
    }

    auto behavior(ScrollInputKind kind) const
        -> std::optional<ScrollInputBehavior> {
      switch (kind) {
        case ScrollInputKind::controller:
          return controller;
        case ScrollInputKind::pointer:
          return pointer;
        case ScrollInputKind::touch:
          break;
      }
      return touch;
    }

    auto accepts(ScrollInputKind kind) const -> bool {
      return behavior(kind) != ScrollInputBehavior::disabled;
    }

    auto over(const Inputs& outer) const -> Inputs {
      return Inputs{
          .touch = touch.has_value() ? touch : outer.touch,
          .controller = controller.has_value() ? controller : outer.controller,
          .pointer = pointer.has_value() ? pointer : outer.pointer};
    }

    auto empty() const noexcept -> bool {
      return !touch.has_value() && !controller.has_value() &&
             !pointer.has_value();
    }

    friend constexpr auto operator==(const Inputs&, const Inputs&)
        -> bool = default;
  };

  struct Anchors {
    std::optional<motion::UnitPoint> initial_offset;
    std::optional<motion::UnitPoint> size_changes;
    std::optional<motion::UnitPoint> alignment;

    auto over(const Anchors& outer) const -> Anchors {
      return Anchors{
          .initial_offset = initial_offset.has_value() ? initial_offset
                                                       : outer.initial_offset,
          .size_changes =
              size_changes.has_value() ? size_changes : outer.size_changes,
          .alignment = alignment.has_value() ? alignment : outer.alignment};
    }

    friend constexpr auto operator==(const Anchors&, const Anchors&)
        -> bool = default;
  };

  struct IgnoredSafeArea {
    SafeAreaRegions regions{SafeAreaRegions::all};
    theme::Edge edges{theme::Edge::all};
    std::optional<Alignment> alignment;

    friend constexpr auto operator==(const IgnoredSafeArea&,
                                     const IgnoredSafeArea&) -> bool = default;
  };

  struct Flash {
    std::optional<ElementKey> trigger;
    bool on_appear{false};

    friend auto operator==(const Flash&, const Flash&) -> bool = default;
  };

  std::optional<Container> container;
  std::optional<bool> disabled;
  std::optional<ScrollTargetBehavior> target_behavior;
  std::optional<ViewAlignedScrollTargetBehavior> aligned_behavior;
  std::optional<AnyScrollTargetBehavior> custom_target_behavior;
  Along<ScrollIndicatorVisibility> indicators;
  Along<ScrollBounceBehavior> bounce;
  std::optional<ScrollDecelerationRate> deceleration;
  std::optional<ScrollDismissesKeyboardMode> dismisses_keyboard;
  Inputs inputs;
  Anchors anchors;
  std::optional<Flash> flash;
  bool target_layout{false};
  std::optional<theme::Insets> content_margins;
  std::optional<theme::Insets> indicator_margins;
  bool clip_disabled{false};
  std::optional<IgnoredSafeArea> ignores_safe_area;
  std::optional<theme::Edge> safe_area_bar;
  ScrollEdgeEffects edge_effects;
  std::vector<std::shared_ptr<PhaseContextAction>> phase_actions;
  std::vector<std::shared_ptr<ScrollGeometryObserver>> geometry_observers;
  std::shared_ptr<ScrollGeometryObserver> nested_geometry;
  std::vector<std::shared_ptr<ScrollVisibilityObserver>> visibility_observers;
  std::vector<std::shared_ptr<ScrollTargetVisibilityObserver>>
      target_visibility_observers;
  std::shared_ptr<ScrollPositionBinding> position;
  std::shared_ptr<const Binding<ScrollPosition>> scroll_position;
  std::optional<motion::UnitPoint> scroll_position_anchor;
  std::shared_ptr<std::move_only_function<void()>> refresh;
};

}
