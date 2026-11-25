#pragma once

#include <arisa/attr/focus.hpp>
#include <arisa/components/stack_options.hpp>
#include <arisa/composition/any_scroll_target_behavior.hpp>
#include <arisa/composition/axes.hpp>
#include <arisa/composition/axis.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/paging_scroll_target_behavior.hpp>
#include <arisa/composition/scroll_anchor_role.hpp>
#include <arisa/composition/scroll_bounce_behavior.hpp>
#include <arisa/composition/scroll_deceleration_rate.hpp>
#include <arisa/composition/scroll_effects.hpp>
#include <arisa/composition/scroll_geometry.hpp>
#include <arisa/composition/scroll_indicator_visibility.hpp>
#include <arisa/composition/scroll_position.hpp>
#include <arisa/composition/scroll_target_behavior.hpp>
#include <arisa/composition/scroll_transition_configuration.hpp>
#include <arisa/composition/scrolling.hpp>
#include <arisa/composition/view_aligned_scroll_target_behavior.hpp>
#include <arisa/composition/visibility.hpp>
#include <arisa/theme/space.hpp>

#include <concepts>
#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

namespace arisa::attr {

class ScrollAxes {
 public:
  constexpr explicit ScrollAxes(composition::Axes axes) noexcept
      : axes_(axes) {}

  constexpr void configure(
      components::ScrollViewOptions& options) const noexcept {
    options.axes = axes_;
  }

 private:
  composition::Axes axes_;
};

class ShowsIndicators {
 public:
  constexpr explicit ShowsIndicators(bool shows = true) noexcept
      : shows_(shows) {}

  constexpr void configure(
      components::ScrollViewOptions& options) const noexcept {
    options.shows_indicators = shows_;
  }

 private:
  bool shows_;
};

class Overscan {
 public:
  constexpr explicit Overscan(float points) noexcept : points_(points) {}

  constexpr void configure(
      components::LazyVStackOptions& options) const noexcept {
    options.overscan = points_;
  }

  constexpr void configure(
      components::LazyHStackOptions& options) const noexcept {
    options.overscan = points_;
  }

  void configure(components::LazyVGridOptions& options) const noexcept {
    options.overscan = points_;
  }

  void configure(components::LazyHGridOptions& options) const noexcept {
    options.overscan = points_;
  }

 private:
  float points_;
};

class EstimatedExtent {
 public:
  constexpr explicit EstimatedExtent(float points) noexcept : points_(points) {}

  constexpr void configure(
      components::LazyVStackOptions& options) const noexcept {
    options.estimated_extent = points_;
  }

  constexpr void configure(
      components::LazyHStackOptions& options) const noexcept {
    options.estimated_extent = points_;
  }

  void configure(components::LazyVGridOptions& options) const noexcept {
    options.estimated_extent = points_;
  }

  void configure(components::LazyHGridOptions& options) const noexcept {
    options.estimated_extent = points_;
  }

 private:
  float points_;
};

class ScrollDisabled {
 public:
  constexpr explicit ScrollDisabled(bool is_disabled = true) noexcept
      : disabled_(is_disabled) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).scroll_disabled(disabled_);
  }

 private:
  bool disabled_;
};

class ScrollTargetBehaviorAttribute {
 public:
  constexpr explicit ScrollTargetBehaviorAttribute(
      composition::ScrollTargetBehavior behavior) noexcept
      : behavior_(behavior) {}

  explicit ScrollTargetBehaviorAttribute(
      composition::PagingScrollTargetBehavior behavior) noexcept
      : behavior_(behavior) {}

  explicit ScrollTargetBehaviorAttribute(
      composition::ViewAlignedScrollTargetBehavior behavior) noexcept
      : behavior_(behavior) {}

  template <class Behavior>
    requires(!std::same_as<std::remove_cvref_t<Behavior>,
                           ScrollTargetBehaviorAttribute> &&
             !std::same_as<std::remove_cvref_t<Behavior>,
                           composition::PagingScrollTargetBehavior> &&
             !std::same_as<std::remove_cvref_t<Behavior>,
                           composition::ViewAlignedScrollTargetBehavior> &&
             composition::CustomScrollTargetBehavior<Behavior>)
  explicit ScrollTargetBehaviorAttribute(Behavior behavior)
      : behavior_(composition::AnyScrollTargetBehavior{std::move(behavior)}) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::visit(
        [&element](const auto& chosen) {
          return std::move(element).scroll_target_behavior(chosen);
        },
        behavior_);
  }

 private:
  std::variant<composition::ScrollTargetBehavior,
               composition::AnyScrollTargetBehavior,
               composition::PagingScrollTargetBehavior,
               composition::ViewAlignedScrollTargetBehavior>
      behavior_;
};

class ScrollIndicators {
 public:
  constexpr explicit ScrollIndicators(
      composition::ScrollIndicatorVisibility visibility,
      composition::Axes along = composition::Axes::all) noexcept
      : visibility_(visibility), axes_(along) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).scroll_indicators(visibility_, axes_);
  }

 private:
  composition::ScrollIndicatorVisibility visibility_;
  composition::Axes axes_;
};

class ScrollIndicatorsFlash : public Deferred {
 public:
  explicit ScrollIndicatorsFlash(bool appearing)
      : Deferred([appearing](composition::Element element) {
          return std::move(element).scroll_indicators_flash(appearing);
        }) {}

  template <composition::Identifiable T>
    requires(!std::same_as<T, bool>)
  explicit ScrollIndicatorsFlash(const T& trigger)
      : Deferred([key = composition::ElementKey::identifying(trigger)](
                     composition::Element element) {
          return std::move(element).scroll_indicators_flash(key);
        }) {}
};

class ScrollBounceBehaviorAttribute {
 public:
  constexpr explicit ScrollBounceBehaviorAttribute(
      composition::ScrollBounceBehavior behavior,
      composition::Axes along = composition::Axes::vertical) noexcept
      : behavior_(behavior), axes_(along) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).scroll_bounce_behavior(behavior_, axes_);
  }

 private:
  composition::ScrollBounceBehavior behavior_;
  composition::Axes axes_;
};

class ScrollDecelerationRateAttribute {
 public:
  constexpr explicit ScrollDecelerationRateAttribute(
      composition::ScrollDecelerationRate rate) noexcept
      : rate_(rate) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).scroll_deceleration_rate(rate_);
  }

 private:
  composition::ScrollDecelerationRate rate_;
};

class DefaultScrollAnchor {
 public:
  constexpr explicit DefaultScrollAnchor(
      std::optional<motion::UnitPoint> anchor) noexcept
      : anchor_(anchor) {}

  constexpr DefaultScrollAnchor(std::optional<motion::UnitPoint> anchor,
                                composition::ScrollAnchorRole purpose) noexcept
      : anchor_(anchor), role_(purpose) {}

  auto apply(composition::Element element) const -> composition::Element {
    if (role_.has_value()) {
      return std::move(element).default_scroll_anchor(anchor_, *role_);
    }
    return std::move(element).default_scroll_anchor(anchor_);
  }

 private:
  std::optional<motion::UnitPoint> anchor_;
  std::optional<composition::ScrollAnchorRole> role_;
};

class ScrollPositionAttribute : public Deferred {
 public:
  explicit ScrollPositionAttribute(
      composition::Binding<composition::ScrollPosition> binding,
      std::optional<motion::UnitPoint> anchor = std::nullopt)
      : Deferred([binding = std::move(binding),
                  anchor](composition::Element element) mutable {
          return std::move(element).scroll_position(std::move(binding), anchor);
        }) {}
};

class ScrollContentBackground {
 public:
  constexpr explicit ScrollContentBackground(
      composition::Visibility visibility) noexcept
      : visibility_(visibility) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).scroll_content_background(visibility_);
  }

 private:
  composition::Visibility visibility_;
};

class ScrollTargetLayout {
 public:
  constexpr explicit ScrollTargetLayout(bool enabled = true) noexcept
      : enabled_(enabled) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).scroll_target_layout(enabled_);
  }

 private:
  bool enabled_;
};

class ContentMargins {
 public:
  explicit ContentMargins(theme::Space length) noexcept
      : insets_(theme::Insets::all(length)) {}

  explicit ContentMargins(theme::Insets insets) noexcept : insets_(insets) {}

  ContentMargins(theme::Edge edges, theme::Space length) noexcept
      : insets_(theme::Insets{}.replacing(edges, length)) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).content_margins(insets_);
  }

 private:
  theme::Insets insets_;
};

class ScrollClipDisabled {
 public:
  constexpr explicit ScrollClipDisabled(bool is_disabled = true) noexcept
      : disabled_(is_disabled) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).scroll_clip_disabled(disabled_);
  }

 private:
  bool disabled_;
};

class ScrollTransition : public Deferred {
 public:
  explicit ScrollTransition(composition::ScrollEffects::Transition body)
      : ScrollTransition(
            composition::ScrollTransitionConfiguration::interactive(),
            std::move(body)) {}

  ScrollTransition(composition::ScrollTransitionConfiguration configuration,
                   composition::ScrollEffects::Transition body)
      : Deferred([configuration, held = std::move(body)](
                     composition::Element element) mutable {
          return std::move(element).scroll_transition(configuration,
                                                      std::move(held));
        }) {}

  ScrollTransition(composition::ScrollTransitionConfiguration configuration,
                   composition::Axis axis,
                   composition::ScrollEffects::Transition body)
      : Deferred([configuration, axis, held = std::move(body)](
                     composition::Element element) mutable {
          return std::move(element).scroll_transition(configuration, axis,
                                                      std::move(held));
        }) {}

  ScrollTransition(composition::ScrollTransitionConfiguration top_leading,
                   composition::ScrollTransitionConfiguration bottom_trailing,
                   composition::ScrollEffects::Transition body)
      : Deferred([top_leading, bottom_trailing, held = std::move(body)](
                     composition::Element element) mutable {
          return std::move(element).scroll_transition(
              top_leading, bottom_trailing, std::move(held));
        }) {}

  ScrollTransition(composition::ScrollTransitionConfiguration top_leading,
                   composition::ScrollTransitionConfiguration bottom_trailing,
                   composition::Axis axis,
                   composition::ScrollEffects::Transition body)
      : Deferred([top_leading, bottom_trailing, axis, held = std::move(body)](
                     composition::Element element) mutable {
          return std::move(element).scroll_transition(
              top_leading, bottom_trailing, axis, std::move(held));
        }) {}
};

class VisualEffectAttribute : public Deferred {
 public:
  explicit VisualEffectAttribute(composition::ScrollEffects::Effect effect)
      : Deferred(
            [effect = std::move(effect)](composition::Element element) mutable {
              return std::move(element).visual_effect(std::move(effect));
            }) {}
};

class OnScrollPhaseChange : public Deferred {
 public:
  explicit OnScrollPhaseChange(composition::Scrolling::PhaseAction perform)
      : Deferred(
            [held = std::move(perform)](composition::Element element) mutable {
              return std::move(element).on_scroll_phase_change(std::move(held));
            }) {}

  explicit OnScrollPhaseChange(
      composition::Scrolling::PhaseContextAction perform)
      : Deferred(
            [held = std::move(perform)](composition::Element element) mutable {
              return std::move(element).on_scroll_phase_change(std::move(held));
            }) {}
};

class OnScrollGeometryChange : public Deferred {
 public:
  template <class Transform, class Action>
    requires std::invocable<const Transform&,
                            const composition::ScrollGeometry&>
  OnScrollGeometryChange(Transform of, Action perform)
      : Deferred([of = std::move(of), held = std::move(perform)](
                     composition::Element element) mutable {
          return std::move(element).on_scroll_geometry_change(std::move(of),
                                                              std::move(held));
        }) {}
};

using axes = ScrollAxes;
using shows_indicators = ShowsIndicators;
using overscan = Overscan;
using estimated_extent = EstimatedExtent;
using scroll_disabled = ScrollDisabled;
using scroll_target_behavior = ScrollTargetBehaviorAttribute;
using scroll_target_layout = ScrollTargetLayout;
using scroll_indicators = ScrollIndicators;
using scroll_indicators_flash = ScrollIndicatorsFlash;
using scroll_bounce_behavior = ScrollBounceBehaviorAttribute;
using scroll_deceleration_rate = ScrollDecelerationRateAttribute;
using default_scroll_anchor = DefaultScrollAnchor;
using scroll_position = ScrollPositionAttribute;
using scroll_content_background = ScrollContentBackground;
using content_margins = ContentMargins;
using safe_area_padding = ContentMargins;
using scroll_clip_disabled = ScrollClipDisabled;
using scroll_transition = ScrollTransition;
using visual_effect = VisualEffectAttribute;
using on_scroll_phase_change = OnScrollPhaseChange;
using on_scroll_geometry_change = OnScrollGeometryChange;

}
