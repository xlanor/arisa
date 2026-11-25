#pragma once

#include <arisa/composition/axis.hpp>
#include <arisa/composition/geometry_proxy.hpp>
#include <arisa/composition/scroll_transition_configuration.hpp>
#include <arisa/composition/scroll_transition_phase.hpp>
#include <arisa/composition/size.hpp>
#include <arisa/composition/visual_effect.hpp>
#include <arisa/graphics/projection_transform.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <vector>

namespace arisa::composition {

struct ScrollEffects {
  using Transition = std::move_only_function<VisualEffect(
      VisualEffect, ScrollTransitionPhase) const>;
  using Effect =
      std::move_only_function<VisualEffect(VisualEffect, GeometryProxy) const>;
  using Projection =
      std::move_only_function<graphics::ProjectionTransform(Size) const>;

  struct ScrollTransition {
    ScrollTransitionConfiguration configuration{
        ScrollTransitionConfiguration::interactive()};
    std::optional<Axis> axis{};
    Transition transition{};
    std::optional<ScrollTransitionConfiguration> bottom_trailing{};

    auto trailing() const noexcept -> const ScrollTransitionConfiguration& {
      return bottom_trailing.has_value() ? *bottom_trailing : configuration;
    }

    auto side(double progress) const noexcept
        -> const ScrollTransitionConfiguration& {
      return progress > 0.0 ? trailing() : configuration;
    }

    auto progress(double start, double end, double container) const noexcept
        -> double {
      using Mode = ScrollTransitionConfiguration::Mode;
      const double length = end - start;
      if (length <= 0.0) {
        return 0.0;
      }
      const bool leads = configuration.mode() != Mode::identity;
      const bool trails = trailing().mode() != Mode::identity;
      const double trailing_from =
          trailing().threshold().trailing_start(container, length);
      const double leading_from =
          configuration.threshold().leading_start(container, length);
      if (trails && container - trailing_from <= 0.0) {
        return 1.0;
      }
      if (leads && leading_from <= 0.0) {
        return -1.0;
      }
      if (leads && end < leading_from) {
        return -std::clamp((leading_from - end) / leading_from, 0.0, 1.0);
      }
      if (trails && start > trailing_from) {
        return std::clamp((start - trailing_from) / (container - trailing_from),
                          0.0, 1.0);
      }
      return 0.0;
    }

    auto shaped(double progress) const noexcept -> double {
      using Mode = ScrollTransitionConfiguration::Mode;
      const ScrollTransitionConfiguration& chosen = side(progress);
      const double sign = progress < 0.0 ? -1.0 : 1.0;
      switch (chosen.mode()) {
        case Mode::interactive:
          return sign * chosen.timing_curve().value(std::abs(progress));
        case Mode::animated:
          return progress == 0.0 ? 0.0 : sign;
        case Mode::identity:
          break;
      }
      return 0.0;
    }
  };

  struct Pin {
    Axis axis{Axis::vertical};
    bool trailing{false};

    friend constexpr auto operator==(const Pin&, const Pin&) -> bool = default;
  };

  std::vector<ScrollTransition> transitions;
  std::vector<Effect> effects;
  std::vector<Projection> projections;
  std::optional<Pin> pin;

  auto empty() const noexcept -> bool {
    return transitions.empty() && effects.empty() && projections.empty() &&
           !pin.has_value();
  }
};

}
