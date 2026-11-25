#pragma once

#include <arisa/motion/animatable_vector.hpp>
#include <arisa/motion/animation_context.hpp>
#include <arisa/motion/custom_animation.hpp>
#include <arisa/motion/spring.hpp>
#include <arisa/motion/unit_curve.hpp>

#include <concepts>
#include <memory>
#include <optional>
#include <utility>
#include <variant>

namespace arisa::motion {

class Animation {
 public:
  struct Timed {
    UnitCurve curve{UnitCurve::linear};
    double duration{0.35};

    friend auto operator==(const Timed&, const Timed&) -> bool = default;
  };

  using Curve =
      std::variant<Spring, Timed, std::shared_ptr<const CustomAnimation>>;

  struct Repetition {
    std::optional<int> count;
    bool autoreverses{true};

    friend auto operator==(const Repetition&, const Repetition&)
        -> bool = default;
  };

  struct Span {
    AnimatableVector from;
    AnimatableVector to;
    AnimatableVector velocity;
    double precision{1.0e-3};
  };

  struct Sample {
    AnimatableVector value;
    AnimatableVector velocity;
    bool finished{false};
  };

  struct Interactive {
    double duration{0.15};
    double extra_bounce{0.0};
    double blend_duration{0.25};
  };

  static constexpr double default_duration = 0.35;
  static constexpr double interactive_bounce = 0.15;
  static constexpr double cross_fade_duration = 0.2;
  static constexpr double minimum_speed = 1.0e-6;

  static auto spring(double response = 0.5, double damping_fraction = 1.0,
                     double blend_duration = 0.0) noexcept -> Animation;

  static auto spring(Spring spring, double blend_duration = 0.0) noexcept
      -> Animation;

  static auto interactive_spring(double response = 0.15,
                                 double damping_fraction = 0.85,
                                 double blend_duration = 0.25) noexcept
      -> Animation;

  static auto interactive_spring(Interactive parameters) noexcept -> Animation;

  static auto interpolating_spring(Spring spring = Spring{},
                                   double initial_velocity = 0.0) noexcept
      -> Animation;

  static auto interpolating_spring(Spring::Physical parameters,
                                   double initial_velocity = 0.0) noexcept
      -> Animation;

  static auto interpolating_spring(Spring::Duration parameters,
                                   double initial_velocity = 0.0) noexcept
      -> Animation;

  static auto smooth(double duration = 0.5, double extra_bounce = 0.0) noexcept
      -> Animation;

  static auto snappy(double duration = 0.5, double extra_bounce = 0.0) noexcept
      -> Animation;

  static auto bouncy(double duration = 0.5, double extra_bounce = 0.0) noexcept
      -> Animation;

  static auto default_animation() noexcept -> Animation;

  static auto custom(std::shared_ptr<const CustomAnimation> animation) noexcept
      -> Animation;

  template <std::derived_from<CustomAnimation> T>
  static auto custom(T animation) -> Animation {
    return custom(std::make_shared<const T>(std::move(animation)));
  }

  static auto linear(double duration = default_duration) noexcept -> Animation;

  static auto ease_in(double duration = default_duration) noexcept -> Animation;

  static auto ease_out(double duration = default_duration) noexcept
      -> Animation;

  static auto ease_in_out(double duration = default_duration) noexcept
      -> Animation;

  static auto timing_curve(double c0x, double c0y, double c1x, double c1y,
                           double duration = default_duration) noexcept
      -> Animation;

  static auto timing_curve(UnitCurve curve,
                           double duration = default_duration) noexcept
      -> Animation;

  auto delay(double seconds) const noexcept -> Animation;

  auto logically_complete(double after) const noexcept -> Animation;

  auto should_merge(const Animation& previous, const AnimatableVector& value,
                    double time, AnimationContext& context) const -> bool;

  auto base() const -> std::shared_ptr<const CustomAnimation>;

  auto additive() const noexcept -> bool { return additive_; }

  auto initial_velocity() const noexcept -> double { return initial_velocity_; }

  auto logically_complete_after() const noexcept -> std::optional<double> {
    return logical_after_;
  }

  auto speed(double factor) const noexcept -> Animation;

  auto repeat_count(int count, bool autoreverses = true) const noexcept
      -> Animation;

  auto repeat_forever(bool autoreverses = true) const noexcept -> Animation;

  auto preferred_frame_rate(double frames_per_second) const noexcept
      -> Animation;

  auto frame_rate() const noexcept -> std::optional<double> {
    return frame_rate_;
  }

  auto curve() const noexcept -> const Curve& { return curve_; }

  auto blend_duration() const noexcept -> double { return blend_duration_; }

  auto delay_seconds() const noexcept -> double { return delay_; }

  auto speed_factor() const noexcept -> double { return speed_; }

  auto repetition() const noexcept -> const std::optional<Repetition>& {
    return repetition_;
  }

  auto spring_parameters() const noexcept -> std::optional<Spring>;

  auto cycle_duration() const noexcept -> double;

  auto logical_duration() const noexcept -> double;

  auto custom_animation() const noexcept -> const CustomAnimation*;

  auto reduced() const noexcept -> Animation;

  auto sample(const Span& span, double elapsed) const noexcept -> Sample;

  auto sample(const Span& span, double elapsed,
              const std::optional<Spring>& blended_from) const noexcept
      -> Sample;

  auto sample(const Span& span, double elapsed,
              const std::optional<Spring>& blended_from,
              AnimationContext& context) const noexcept -> Sample;

  auto spring_at(double elapsed,
                 const std::optional<Spring>& blended_from) const noexcept
      -> std::optional<Spring>;

  friend auto operator==(const Animation&, const Animation&) -> bool = default;

 private:
  struct Progress {
    double value{0.0};
    double rate{0.0};
  };

  explicit Animation(Curve curve, double blend_duration = 0.0) noexcept
      : curve_(std::move(curve)), blend_duration_(blend_duration) {}

  static auto timed(UnitCurve curve, double duration) noexcept -> Animation;

  auto physical(const Spring& spring, const Span& span, double time,
                const std::optional<Spring>& blended_from) const noexcept
      -> Sample;

  auto progress(double time, double cycle) const noexcept -> Progress;

  auto invented(const CustomAnimation& animation, const Span& span, double time,
                AnimationContext& context) const noexcept -> Sample;

  Curve curve_;
  double blend_duration_{0.0};
  double delay_{0.0};
  double speed_{1.0};
  std::optional<Repetition> repetition_;
  std::optional<double> frame_rate_;
  std::optional<double> logical_after_;
  double initial_velocity_{0.0};
  bool additive_{false};
};

}
