#pragma once

#include <arisa/composition/any_scroll_target_behavior.hpp>
#include <arisa/composition/axes.hpp>
#include <arisa/composition/environment_values.hpp>
#include <arisa/composition/paging_scroll_target_behavior.hpp>
#include <arisa/composition/point.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/composition/scroll_phase.hpp>
#include <arisa/composition/scroll_target_behavior.hpp>
#include <arisa/composition/scroll_target_behavior_properties.hpp>
#include <arisa/composition/size.hpp>
#include <arisa/composition/view_aligned_scroll_target_behavior.hpp>
#include <arisa/motion/animatable_vector.hpp>
#include <arisa/motion/animation.hpp>

#include <boost/sml.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

namespace arisa::interaction {

template <class... Policies>
class BasicScroller {
 public:
  struct Metrics {
    composition::Size viewport{};
    composition::Size content{};
    composition::Axes axes{composition::Axes::vertical};
    composition::ScrollTargetBehavior behavior{
        composition::ScrollTargetBehavior::standard};
    std::vector<float> targets;
    double rate{normal_rate};
    bool bounces_horizontally{true};
    bool bounces_vertically{true};
    std::optional<composition::AnyScrollTargetBehavior> custom{};
    std::vector<float> extents{};
    float margin{0.0F};
    composition::ViewAlignedScrollTargetBehavior aligned{};
    bool horizontally_compact{true};
    composition::EnvironmentValues environment{};
  };

  static constexpr double normal_rate = 0.998;
  static constexpr double fast_rate = 0.99;
  static constexpr double rubber_coefficient = 0.55;
  static constexpr double stop_speed = 8.0;
  static constexpr double fling_speed = 40.0;
  static constexpr double settle_response = 0.45;
  static constexpr double settle_damping = 1.0;
  static constexpr double settle_precision = 0.25;
  static constexpr std::chrono::milliseconds indicator_hold{600};
  static constexpr std::chrono::milliseconds indicator_fade{300};
  static constexpr std::chrono::milliseconds interaction_window{100};

  static auto rubber_band(double overshoot, double dimension) noexcept
      -> double {
    if (overshoot <= 0.0 || dimension <= 0.0) {
      return 0.0;
    }
    return (1.0 -
            (1.0 / ((overshoot * rubber_coefficient / dimension) + 1.0))) *
           dimension;
  }

  static auto decay_displacement(double velocity, double rate,
                                 double seconds) noexcept -> double {
    const double logarithm = std::log(rate);
    return velocity * (std::pow(rate, 1000.0 * seconds) - 1.0) /
           (1000.0 * logarithm);
  }

  static auto decay_velocity(double velocity, double rate,
                             double seconds) noexcept -> double {
    return velocity * std::pow(rate, 1000.0 * seconds);
  }

  static auto projection(double velocity, double rate) noexcept -> double {
    return -velocity / (1000.0 * std::log(rate));
  }

  static auto settle() -> motion::Animation {
    return motion::Animation::spring(settle_response, settle_damping);
  }

  explicit BasicScroller(double rate = normal_rate) noexcept {
    motion_.rate = rate;
    motion_.metrics.rate = rate;
  }

  void configure(const Metrics& metrics) {
    motion_.metrics = metrics;
    std::ranges::sort(motion_.metrics.targets);
    if (machine_.is(boost::sml::state<Idle>)) {
      motion_.rate = metrics.rate;
      motion_.x = clamp_x(motion_.x);
      motion_.y = clamp_y(motion_.y);
    }
  }

  void reposition(composition::Point offset) noexcept {
    if (!machine_.is(boost::sml::state<Idle>)) {
      return;
    }
    motion_.x = clamp_x(offset.x);
    motion_.y = clamp_y(offset.y);
  }

  void flash(std::chrono::nanoseconds now) noexcept { motion_.active = now; }

  void grab(std::chrono::nanoseconds now) {
    machine_.process_event(Grab{.motion = &motion_, .now = now});
  }

  void drag(composition::Size translation, std::chrono::nanoseconds now) {
    machine_.process_event(
        Drag{.motion = &motion_, .translation = translation, .now = now});
  }

  void release(composition::Size velocity, std::chrono::nanoseconds now) {
    machine_.process_event(
        Release{.motion = &motion_, .velocity = velocity, .now = now});
  }

  void animate(composition::Point target,
               std::optional<motion::Animation> animation,
               std::chrono::nanoseconds now) {
    machine_.process_event(Animate{.motion = &motion_,
                                   .target = target,
                                   .animation = std::move(animation),
                                   .now = now});
  }

  auto carry(composition::Point target, std::chrono::nanoseconds now) noexcept
      -> bool {
    if (!decelerating()) {
      return false;
    }
    const double shift_x = clamp_x(target.x) - motion_.x;
    const double shift_y = clamp_y(target.y) - motion_.y;
    motion_.x += shift_x;
    motion_.y += shift_y;
    motion_.from_x += shift_x;
    motion_.from_y += shift_y;
    motion_.active = now;
    return true;
  }

  auto nudge(composition::Point delta, std::chrono::nanoseconds now) -> bool {
    const composition::Point before = offset();
    machine_.process_event(
        Nudge{.motion = &motion_, .delta = delta, .now = now});
    return offset() != before;
  }

  void advance(std::chrono::nanoseconds now) {
    if (!machine_.is(boost::sml::state<Idle>)) {
      motion_.active = now;
    }
    machine_.process_event(Advance{.motion = &motion_, .now = now});
    machine_.process_event(Settle{.motion = &motion_});
  }

  auto offset() const noexcept -> composition::Point {
    return composition::Point{.x = static_cast<float>(motion_.x),
                              .y = static_cast<float>(motion_.y)};
  }

  auto velocity() const noexcept -> composition::Size {
    return composition::Size{.width = static_cast<float>(motion_.vx),
                             .height = static_cast<float>(motion_.vy)};
  }

  auto target() const noexcept -> composition::Point {
    if (machine_.is(boost::sml::state<Animating>) ||
        machine_.is(boost::sml::state<Bouncing>)) {
      return composition::Point{.x = static_cast<float>(motion_.to_x),
                                .y = static_cast<float>(motion_.to_y)};
    }
    if (machine_.is(boost::sml::state<Decelerating>)) {
      return composition::Point{
          .x = static_cast<float>(
              clamp_x(motion_.x + projection(motion_.vx, motion_.rate))),
          .y = static_cast<float>(
              clamp_y(motion_.y + projection(motion_.vy, motion_.rate)))};
    }
    return offset();
  }

  auto limit() const noexcept -> composition::Point {
    return composition::Point{.x = static_cast<float>(max_x()),
                              .y = static_cast<float>(max_y())};
  }

  auto metrics() const noexcept -> const Metrics& { return motion_.metrics; }

  auto idle() const noexcept -> bool {
    return machine_.is(boost::sml::state<Idle>);
  }

  auto dragging() const noexcept -> bool {
    return machine_.is(boost::sml::state<Dragging>);
  }

  auto decelerating() const noexcept -> bool {
    return machine_.is(boost::sml::state<Decelerating>);
  }

  auto bouncing() const noexcept -> bool {
    return machine_.is(boost::sml::state<Bouncing>);
  }

  auto animating() const noexcept -> bool {
    return machine_.is(boost::sml::state<Animating>);
  }

  auto phase(std::chrono::nanoseconds now) const noexcept
      -> composition::ScrollPhase {
    if (dragging()) {
      return composition::ScrollPhase::interacting;
    }
    if (decelerating() || bouncing()) {
      return composition::ScrollPhase::decelerating;
    }
    if (animating()) {
      return composition::ScrollPhase::animating;
    }
    if (motion_.nudged.has_value() &&
        now - *motion_.nudged < interaction_window) {
      return composition::ScrollPhase::interacting;
    }
    return composition::ScrollPhase::idle;
  }

  auto indicators(std::chrono::nanoseconds now) const noexcept -> float {
    if (!idle()) {
      return 1.0F;
    }
    if (!motion_.active.has_value()) {
      return 0.0F;
    }
    const std::chrono::nanoseconds rested = now - *motion_.active;
    if (rested <= indicator_hold) {
      return 1.0F;
    }
    const double faded =
        std::chrono::duration<double>(rested - indicator_hold).count() /
        std::chrono::duration<double>(indicator_fade).count();
    return static_cast<float>(std::clamp(1.0 - faded, 0.0, 1.0));
  }

  auto moving(std::chrono::nanoseconds now) const noexcept -> bool {
    return !idle() || indicators(now) > 0.0F ||
           phase(now) != composition::ScrollPhase::idle;
  }

 private:
  struct Idle {};
  struct Dragging {};
  struct Decelerating {};
  struct Bouncing {};
  struct Animating {};

  struct Motion {
    Metrics metrics;
    double rate{normal_rate};
    double x{0.0};
    double y{0.0};
    double vx{0.0};
    double vy{0.0};
    double base_x{0.0};
    double base_y{0.0};
    double from_x{0.0};
    double from_y{0.0};
    double to_x{0.0};
    double to_y{0.0};
    double start_vx{0.0};
    double start_vy{0.0};
    std::chrono::nanoseconds started{0};
    std::optional<motion::Animation> animation;
    std::optional<std::chrono::nanoseconds> active;
    std::optional<std::chrono::nanoseconds> nudged;
    std::chrono::nanoseconds clock{0};
    bool crossed{false};
    bool slowed{false};
    bool landed{false};

    void launch(composition::Size velocity, std::chrono::nanoseconds now) {
      vx = horizontal() ? -velocity.width : 0.0;
      vy = vertical() ? -velocity.height : 0.0;
      active = now;
    }

    auto horizontal() const noexcept -> bool {
      return metrics.axes.contains(composition::Axis::horizontal);
    }

    auto vertical() const noexcept -> bool {
      return metrics.axes.contains(composition::Axis::vertical);
    }

    auto max_x() const noexcept -> double {
      return horizontal()
                 ? std::max(0.0, static_cast<double>(metrics.content.width -
                                                     metrics.viewport.width))
                 : 0.0;
    }

    auto max_y() const noexcept -> double {
      return vertical()
                 ? std::max(0.0, static_cast<double>(metrics.content.height -
                                                     metrics.viewport.height))
                 : 0.0;
    }

    auto outside() const noexcept -> bool {
      constexpr double tolerance = 0.01;
      return x < -tolerance || y < -tolerance || x > max_x() + tolerance ||
             y > max_y() + tolerance;
    }

    void spring_to(double target_x, double target_y,
                   std::chrono::nanoseconds now, motion::Animation curve) {
      from_x = x;
      from_y = y;
      to_x = std::clamp(target_x, 0.0, max_x());
      to_y = std::clamp(target_y, 0.0, max_y());
      start_vx = vx;
      start_vy = vy;
      started = now;
      animation = curve;
      landed = false;
    }

    auto bounces(bool along_x) const noexcept -> bool {
      return along_x ? metrics.bounces_horizontally
                     : metrics.bounces_vertically;
    }

    void pin_edges() noexcept {
      if (!bounces(true) && (x < 0.0 || x > max_x())) {
        x = std::clamp(x, 0.0, max_x());
        from_x = x;
        vx = 0.0;
        start_vx = 0.0;
      }
      if (!bounces(false) && (y < 0.0 || y > max_y())) {
        y = std::clamp(y, 0.0, max_y());
        from_y = y;
        vy = 0.0;
        start_vy = 0.0;
      }
    }

    auto described() const -> composition::ScrollTargetBehaviorProperties {
      const composition::ScrollTargetBehaviorPropertiesContext context{
          .axes = metrics.axes,
          .horizontally_compact = metrics.horizontally_compact,
          .environment = metrics.environment};
      if (metrics.custom.has_value()) {
        return metrics.custom->properties(context);
      }
      switch (metrics.behavior) {
        case composition::ScrollTargetBehavior::paging:
          return composition::PagingScrollTargetBehavior{}.properties(context);
        case composition::ScrollTargetBehavior::view_aligned:
          return metrics.aligned.properties(context);
        case composition::ScrollTargetBehavior::standard:
          break;
      }
      return composition::ScrollTargetBehaviorProperties{};
    }

    auto snapped() const -> std::pair<double, double> {
      double projected_x = std::clamp(x + projection(vx, rate), 0.0, max_x());
      double projected_y = std::clamp(y + projection(vy, rate), 0.0, max_y());
      if (described().limits_scrolls) {
        projected_x = std::clamp(projected_x, base_x - metrics.viewport.width,
                                 base_x + metrics.viewport.width);
        projected_y = std::clamp(projected_y, base_y - metrics.viewport.height,
                                 base_y + metrics.viewport.height);
      }
      std::vector<composition::Rect> views;
      views.reserve(metrics.targets.size());
      for (std::size_t index = 0; index < metrics.targets.size(); ++index) {
        const float extent =
            index < metrics.extents.size() ? metrics.extents[index] : 0.0F;
        if (vertical()) {
          views.push_back(composition::Rect{.x = 0.0F,
                                            .y = metrics.targets[index],
                                            .width = metrics.viewport.width,
                                            .height = extent});
        } else {
          views.push_back(composition::Rect{.x = metrics.targets[index],
                                            .y = 0.0F,
                                            .width = extent,
                                            .height = metrics.viewport.height});
        }
      }
      const composition::ScrollTarget original{
          .rect = composition::Rect{.x = static_cast<float>(projected_x),
                                    .y = static_cast<float>(projected_y),
                                    .width = metrics.viewport.width,
                                    .height = metrics.viewport.height},
          .anchor = std::nullopt};
      composition::ScrollTarget target = original;
      const composition::ScrollTargetContext context{
          .original_target = original,
          .velocity = composition::Size{.width = static_cast<float>(vx),
                                        .height = static_cast<float>(vy)},
          .content_size = metrics.content,
          .container_size = metrics.viewport,
          .axes = metrics.axes,
          .starting_offset =
              composition::Point{.x = static_cast<float>(base_x),
                                 .y = static_cast<float>(base_y)},
          .current_offset = composition::Point{.x = static_cast<float>(x),
                                               .y = static_cast<float>(y)},
          .aligned_views = views,
          .aligned_margin = metrics.margin,
          .horizontally_compact = metrics.horizontally_compact};
      if (metrics.custom.has_value()) {
        metrics.custom->update_target(target, context);
      } else if (metrics.behavior ==
                 composition::ScrollTargetBehavior::paging) {
        composition::PagingScrollTargetBehavior{}.update_target(target,
                                                                context);
      } else if (metrics.behavior ==
                 composition::ScrollTargetBehavior::view_aligned) {
        metrics.aligned.update_target(target, context);
      }
      double chosen_x = target.rect.x;
      double chosen_y = target.rect.y;
      if (target.anchor.has_value()) {
        chosen_x = target.rect.x + (target.anchor->x * target.rect.width) -
                   (target.anchor->x * metrics.viewport.width);
        chosen_y = target.rect.y + (target.anchor->y * target.rect.height) -
                   (target.anchor->y * metrics.viewport.height);
      }
      return {horizontal() ? std::clamp(chosen_x, 0.0, max_x()) : x,
              vertical() ? std::clamp(chosen_y, 0.0, max_y()) : y};
    }
  };

  struct Grab {
    Motion* motion{nullptr};
    std::chrono::nanoseconds now{0};
  };

  struct Drag {
    Motion* motion{nullptr};
    composition::Size translation{};
    std::chrono::nanoseconds now{0};
  };

  struct Release {
    Motion* motion{nullptr};
    composition::Size velocity{};
    std::chrono::nanoseconds now{0};
  };

  struct Animate {
    Motion* motion{nullptr};
    composition::Point target{};
    std::optional<motion::Animation> animation;
    std::chrono::nanoseconds now{0};
  };

  struct Nudge {
    Motion* motion{nullptr};
    composition::Point delta{};
    std::chrono::nanoseconds now{0};
  };

  struct Advance {
    Motion* motion{nullptr};
    std::chrono::nanoseconds now{0};
  };

  struct Settle {
    Motion* motion{nullptr};
  };

  struct Table {
    auto operator()() const noexcept {
      using boost::sml::event;
      using boost::sml::state;
      const auto hold = [](const Grab& grab) {
        Motion& motion = *grab.motion;
        motion.base_x = motion.x;
        motion.base_y = motion.y;
        motion.vx = 0.0;
        motion.vy = 0.0;
        motion.active = grab.now;
      };
      const auto follow = [](const Drag& drag) {
        Motion& motion = *drag.motion;
        const auto band = [](double raw, double upper, double dimension,
                             bool elastic) {
          if (!elastic) {
            return std::clamp(raw, 0.0, upper);
          }
          if (raw < 0.0) {
            return -rubber_band(-raw, dimension);
          }
          if (raw > upper) {
            return upper + rubber_band(raw - upper, dimension);
          }
          return raw;
        };
        if (motion.horizontal()) {
          motion.x =
              band(motion.base_x - drag.translation.width, motion.max_x(),
                   motion.metrics.viewport.width, motion.bounces(true));
        }
        if (motion.vertical()) {
          motion.y =
              band(motion.base_y - drag.translation.height, motion.max_y(),
                   motion.metrics.viewport.height, motion.bounces(false));
        }
        motion.active = drag.now;
      };
      const auto outside = [](const Release& release) {
        return release.motion->outside();
      };
      const auto snapping = [](const Release& release) {
        return release.motion->metrics.behavior !=
                   composition::ScrollTargetBehavior::standard ||
               release.motion->metrics.custom.has_value();
      };
      const auto flung = [](const Release& release) {
        const Motion& motion = *release.motion;
        const double vx = motion.horizontal() ? release.velocity.width : 0.0;
        const double vy = motion.vertical() ? release.velocity.height : 0.0;
        return std::hypot(vx, vy) > fling_speed;
      };
      const auto bounce = [](const Release& release) {
        Motion& motion = *release.motion;
        motion.launch(release.velocity, release.now);
        motion.spring_to(motion.x, motion.y, release.now, settle());
      };
      const auto snap = [](const Release& release) {
        Motion& motion = *release.motion;
        motion.launch(release.velocity, release.now);
        const auto [target_x, target_y] = motion.snapped();
        motion.spring_to(target_x, target_y, release.now, settle());
      };
      const auto fling = [](const Release& release) {
        Motion& motion = *release.motion;
        motion.launch(release.velocity, release.now);
        motion.from_x = motion.x;
        motion.from_y = motion.y;
        motion.start_vx = motion.vx;
        motion.start_vy = motion.vy;
        motion.started = release.now;
        motion.crossed = false;
        motion.slowed = false;
      };
      const auto rest = [](const Release& release) {
        release.motion->vx = 0.0;
        release.motion->vy = 0.0;
        release.motion->active = release.now;
      };
      const auto glide = [](const Advance& advance) {
        Motion& motion = *advance.motion;
        const double seconds =
            std::chrono::duration<double>(advance.now - motion.started).count();
        motion.x = motion.from_x +
                   decay_displacement(motion.start_vx, motion.rate, seconds);
        motion.y = motion.from_y +
                   decay_displacement(motion.start_vy, motion.rate, seconds);
        motion.vx = decay_velocity(motion.start_vx, motion.rate, seconds);
        motion.vy = decay_velocity(motion.start_vy, motion.rate, seconds);
        motion.pin_edges();
        motion.crossed = motion.outside();
        motion.slowed = std::hypot(motion.vx, motion.vy) < stop_speed;
        motion.clock = advance.now;
      };
      const auto crossed = [](const Settle& settle_event) {
        return settle_event.motion->crossed;
      };
      const auto slowed = [](const Settle& settle_event) {
        return settle_event.motion->slowed;
      };
      const auto rebound = [](const Settle& settle_event) {
        Motion& motion = *settle_event.motion;
        motion.spring_to(motion.x, motion.y, motion.clock, settle());
      };
      const auto spring = [](const Advance& advance) {
        Motion& motion = *advance.motion;
        if (!motion.animation.has_value()) {
          motion.landed = true;
          return;
        }
        const double seconds =
            std::chrono::duration<double>(advance.now - motion.started).count();
        const motion::Animation::Sample sample = motion.animation->sample(
            motion::Animation::Span{
                .from = motion::AnimatableVector{motion.from_x, motion.from_y},
                .to = motion::AnimatableVector{motion.to_x, motion.to_y},
                .velocity =
                    motion::AnimatableVector{motion.start_vx, motion.start_vy},
                .precision = settle_precision},
            seconds);
        motion.x = sample.value[0];
        motion.y = sample.value[1];
        motion.vx = sample.velocity[0];
        motion.vy = sample.velocity[1];
        motion.landed = sample.finished;
      };
      const auto landed = [](const Settle& settle_event) {
        return settle_event.motion->landed;
      };
      const auto land = [](const Settle& settle_event) {
        Motion& motion = *settle_event.motion;
        motion.x = motion.to_x;
        motion.y = motion.to_y;
        motion.vx = 0.0;
        motion.vy = 0.0;
      };
      const auto halt = [](const Settle& settle_event) {
        settle_event.motion->vx = 0.0;
        settle_event.motion->vy = 0.0;
      };
      const auto animated = [](const Animate& animate) {
        return animate.animation.has_value();
      };
      const auto aim = [](const Animate& animate) {
        Motion& motion = *animate.motion;
        motion.spring_to(animate.target.x, animate.target.y, animate.now,
                         *animate.animation);
        motion.active = animate.now;
      };
      const auto jump = [](const Animate& animate) {
        Motion& motion = *animate.motion;
        motion.x = std::clamp(static_cast<double>(animate.target.x), 0.0,
                              motion.max_x());
        motion.y = std::clamp(static_cast<double>(animate.target.y), 0.0,
                              motion.max_y());
        motion.vx = 0.0;
        motion.vy = 0.0;
        motion.active = animate.now;
      };
      const auto shift = [](const Nudge& nudge) {
        Motion& motion = *nudge.motion;
        const double x =
            std::clamp(motion.x + nudge.delta.x, 0.0, motion.max_x());
        const double y =
            std::clamp(motion.y + nudge.delta.y, 0.0, motion.max_y());
        motion.vx = 0.0;
        motion.vy = 0.0;
        if (x == motion.x && y == motion.y) {
          return;
        }
        motion.x = x;
        motion.y = y;
        motion.active = nudge.now;
        motion.nudged = nudge.now;
      };
      return boost::sml::make_transition_table(
          *state<Idle> + event<Grab> / hold = state<Dragging>,
          state<Idle> + event<Animate>[animated] / aim = state<Animating>,
          state<Idle> + event<Animate> / jump,
          state<Idle> + event<Nudge> / shift,
          state<Dragging> + event<Drag> / follow,
          state<Dragging> + event<Release>[outside] / bounce = state<Bouncing>,
          state<Dragging> + event<Release>[snapping] / snap = state<Animating>,
          state<Dragging> + event<Release>[flung] / fling = state<Decelerating>,
          state<Dragging> + event<Release> / rest = state<Idle>,
          state<Decelerating> + event<Grab> / hold = state<Dragging>,
          state<Decelerating> + event<Advance> / glide,
          state<Decelerating> + event<Settle>[crossed] / rebound =
              state<Bouncing>,
          state<Decelerating> + event<Settle>[slowed] / halt = state<Idle>,
          state<Decelerating> + event<Animate>[animated] / aim =
              state<Animating>,
          state<Decelerating> + event<Animate> / jump = state<Idle>,
          state<Decelerating> + event<Nudge> / shift = state<Idle>,
          state<Bouncing> + event<Grab> / hold = state<Dragging>,
          state<Bouncing> + event<Advance> / spring,
          state<Bouncing> + event<Settle>[landed] / land = state<Idle>,
          state<Bouncing> + event<Animate>[animated] / aim = state<Animating>,
          state<Bouncing> + event<Animate> / jump = state<Idle>,
          state<Bouncing> + event<Nudge> / shift = state<Idle>,
          state<Animating> + event<Grab> / hold = state<Dragging>,
          state<Animating> + event<Advance> / spring,
          state<Animating> + event<Settle>[landed] / land = state<Idle>,
          state<Animating> + event<Animate>[animated] / aim,
          state<Animating> + event<Animate> / jump = state<Idle>,
          state<Animating> + event<Nudge> / shift = state<Idle>);
    }
  };

  auto max_x() const noexcept -> double { return motion_.max_x(); }

  auto max_y() const noexcept -> double { return motion_.max_y(); }

  auto clamp_x(double value) const noexcept -> double {
    return std::clamp(value, 0.0, max_x());
  }

  auto clamp_y(double value) const noexcept -> double {
    return std::clamp(value, 0.0, max_y());
  }

  Motion motion_;
  boost::sml::sm<Table, Policies...> machine_;
};

using Scroller = BasicScroller<>;

}
