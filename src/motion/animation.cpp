#include <arisa/motion/animation.hpp>
#include <arisa/motion/base_animation.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <utility>

namespace arisa::motion {

auto Animation::spring(double response, double damping_fraction,
                       double blend_duration) noexcept -> Animation {
  return Animation{Spring{Spring::Response{.response = response,
                                           .damping_ratio = damping_fraction}},
                   std::max(blend_duration, 0.0)};
}

auto Animation::spring(Spring spring, double blend_duration) noexcept
    -> Animation {
  return Animation{spring, std::max(blend_duration, 0.0)};
}

auto Animation::interactive_spring(double response, double damping_fraction,
                                   double blend_duration) noexcept
    -> Animation {
  return spring(response, damping_fraction, blend_duration);
}

auto Animation::interactive_spring(Interactive parameters) noexcept
    -> Animation {
  return spring(Spring{Spring::Duration{
                    .duration = parameters.duration,
                    .bounce = interactive_bounce + parameters.extra_bounce}},
                parameters.blend_duration);
}

auto Animation::interpolating_spring(Spring spring,
                                     double initial_velocity) noexcept
    -> Animation {
  Animation additive{spring};
  additive.additive_ = true;
  additive.initial_velocity_ = initial_velocity;
  return additive;
}

auto Animation::interpolating_spring(Spring::Physical parameters,
                                     double initial_velocity) noexcept
    -> Animation {
  parameters.allow_over_damping = true;
  return interpolating_spring(Spring{parameters}, initial_velocity);
}

auto Animation::interpolating_spring(Spring::Duration parameters,
                                     double initial_velocity) noexcept
    -> Animation {
  return interpolating_spring(Spring{parameters}, initial_velocity);
}

auto Animation::smooth(double duration, double extra_bounce) noexcept
    -> Animation {
  return spring(
      Spring{Spring::Duration{.duration = duration, .bounce = extra_bounce}});
}

auto Animation::snappy(double duration, double extra_bounce) noexcept
    -> Animation {
  constexpr double bounce = 0.15;
  return spring(Spring{
      Spring::Duration{.duration = duration, .bounce = bounce + extra_bounce}});
}

auto Animation::bouncy(double duration, double extra_bounce) noexcept
    -> Animation {
  constexpr double bounce = 0.3;
  return spring(Spring{
      Spring::Duration{.duration = duration, .bounce = bounce + extra_bounce}});
}

auto Animation::default_animation() noexcept -> Animation { return smooth(); }

auto Animation::custom(
    std::shared_ptr<const CustomAnimation> animation) noexcept -> Animation {
  return Animation{std::move(animation)};
}

auto Animation::timed(UnitCurve curve, double duration) noexcept -> Animation {
  return Animation{Timed{.curve = curve, .duration = std::max(duration, 0.0)}};
}

auto Animation::linear(double duration) noexcept -> Animation {
  return timed(UnitCurve::linear, duration);
}

auto Animation::ease_in(double duration) noexcept -> Animation {
  return timed(UnitCurve::ease_in, duration);
}

auto Animation::ease_out(double duration) noexcept -> Animation {
  return timed(UnitCurve::ease_out, duration);
}

auto Animation::ease_in_out(double duration) noexcept -> Animation {
  return timed(UnitCurve::ease_in_out, duration);
}

auto Animation::timing_curve(double c0x, double c0y, double c1x, double c1y,
                             double duration) noexcept -> Animation {
  return timed(UnitCurve::bezier(c0x, c0y, c1x, c1y), duration);
}

auto Animation::timing_curve(UnitCurve curve, double duration) noexcept
    -> Animation {
  return timed(curve, duration);
}

auto Animation::delay(double seconds) const noexcept -> Animation {
  Animation delayed = *this;
  delayed.delay_ += std::max(seconds, 0.0);
  if (delayed.logical_after_.has_value()) {
    *delayed.logical_after_ += std::max(seconds, 0.0);
  }
  return delayed;
}

auto Animation::speed(double factor) const noexcept -> Animation {
  Animation faster = *this;
  faster.speed_ = std::max(speed_ * factor, minimum_speed);
  if (faster.logical_after_.has_value()) {
    faster.logical_after_ =
        delay_ + ((*logical_after_ - delay_) * speed_ / faster.speed_);
  }
  return faster;
}

auto Animation::logically_complete(double after) const noexcept -> Animation {
  Animation marked = *this;
  marked.logical_after_ = std::max(after, 0.0);
  return marked;
}

auto Animation::should_merge(const Animation& previous,
                             const AnimatableVector& value, double time,
                             AnimationContext& context) const -> bool {
  if (const CustomAnimation* invention = custom_animation();
      invention != nullptr) {
    return invention->should_merge(previous, value, time, context);
  }
  return std::holds_alternative<Spring>(curve_) && !additive_ &&
         !repetition_.has_value() && delay_ == 0.0 && speed_ == 1.0 &&
         !logical_after_.has_value();
}

auto Animation::base() const -> std::shared_ptr<const CustomAnimation> {
  if (const auto* found =
          std::get_if<std::shared_ptr<const CustomAnimation>>(&curve_)) {
    return *found;
  }
  return std::make_shared<const BaseAnimation>(*this);
}

auto Animation::repeat_count(int count, bool autoreverses) const noexcept
    -> Animation {
  Animation repeated = *this;
  repeated.repetition_ =
      Repetition{.count = std::max(count, 1), .autoreverses = autoreverses};
  return repeated;
}

auto Animation::repeat_forever(bool autoreverses) const noexcept -> Animation {
  Animation repeated = *this;
  repeated.repetition_ =
      Repetition{.count = std::nullopt, .autoreverses = autoreverses};
  return repeated;
}

auto Animation::preferred_frame_rate(double frames_per_second) const noexcept
    -> Animation {
  Animation hinted = *this;
  if (frames_per_second > 0.0) {
    hinted.frame_rate_ = frames_per_second;
  }
  return hinted;
}

auto Animation::spring_parameters() const noexcept -> std::optional<Spring> {
  if (const auto* found = std::get_if<Spring>(&curve_)) {
    return *found;
  }
  return std::nullopt;
}

auto Animation::cycle_duration() const noexcept -> double {
  if (const auto* found = std::get_if<Spring>(&curve_)) {
    return found->settling_duration();
  }
  if (const auto* found = std::get_if<Timed>(&curve_)) {
    return found->duration;
  }
  return std::numeric_limits<double>::infinity();
}

auto Animation::logical_duration() const noexcept -> double {
  if (logical_after_.has_value()) {
    return *logical_after_;
  }
  if (custom_animation() != nullptr) {
    return std::numeric_limits<double>::infinity();
  }
  double active = cycle_duration();
  if (repetition_.has_value()) {
    if (!repetition_->count.has_value()) {
      return std::numeric_limits<double>::infinity();
    }
    active *= static_cast<double>(*repetition_->count);
  } else if (const auto* found = std::get_if<Spring>(&curve_)) {
    active = std::min(found->response(), active);
  }
  return delay_ + (active / speed_);
}

auto Animation::custom_animation() const noexcept -> const CustomAnimation* {
  if (const auto* found =
          std::get_if<std::shared_ptr<const CustomAnimation>>(&curve_)) {
    return found->get();
  }
  return nullptr;
}

auto Animation::reduced() const noexcept -> Animation {
  if (!std::holds_alternative<Spring>(curve_)) {
    return *this;
  }
  Animation fade = ease_in_out(cross_fade_duration);
  fade.delay_ = delay_;
  fade.speed_ = speed_;
  fade.logical_after_ = logical_after_;
  return fade;
}

auto Animation::physical(
    const Spring& spring, const Span& span, double time,
    const std::optional<Spring>& blended_from) const noexcept -> Sample {
  Sample sample{.value = span.to, .velocity = {}, .finished = true};
  std::array<double, AnimatableVector::capacity> values{};
  std::array<double, AnimatableVector::capacity> velocities{};
  for (std::size_t index = 0; index < AnimatableVector::capacity; ++index) {
    const double displacement = span.from[index] - span.to[index];
    const double velocity =
        (span.velocity[index] / speed_) +
        (initial_velocity_ * (span.to[index] - span.from[index]));
    const bool blending = blended_from.has_value() && blend_duration_ > 0.0;
    const Spring::State state =
        blending ? spring.blended_state(*blended_from, blend_duration_,
                                        displacement, velocity, time)
                 : spring.state(displacement, velocity, time);
    values[index] = span.to[index] + state.displacement;
    velocities[index] = state.velocity * speed_;
    if ((blending && time < blend_duration_) ||
        !spring.settled(state, span.precision)) {
      sample.finished = false;
    }
  }
  if (!sample.finished) {
    sample.value = AnimatableVector{values[0], values[1], values[2], values[3]};
    sample.velocity = AnimatableVector{velocities[0], velocities[1],
                                       velocities[2], velocities[3]};
  }
  return sample;
}

auto Animation::invented(const CustomAnimation& animation, const Span& span,
                         double time, AnimationContext& context) const noexcept
    -> Sample {
  const AnimatableVector distance = span.to - span.from;
  const std::optional<AnimatableVector> moved =
      animation.animate(distance, time, context);
  if (!moved.has_value()) {
    return Sample{.value = span.to, .velocity = {}, .finished = true};
  }
  const std::optional<AnimatableVector> rate =
      animation.velocity(distance, time, context);
  return Sample{.value = span.from + *moved,
                .velocity = rate.value_or(AnimatableVector{}) * speed_,
                .finished = false};
}

auto Animation::progress(double time, double cycle) const noexcept -> Progress {
  if (const auto* found = std::get_if<Spring>(&curve_)) {
    const Spring::State state = found->state(-1.0, 0.0, time);
    return Progress{.value = 1.0 + state.displacement, .rate = state.velocity};
  }
  const Timed& timing = std::get<Timed>(curve_);
  const double unit = time / cycle;
  return Progress{.value = timing.curve.value(unit),
                  .rate = timing.curve.velocity(unit) / cycle};
}

auto Animation::sample(const Span& span, double elapsed) const noexcept
    -> Sample {
  return sample(span, elapsed, std::nullopt);
}

auto Animation::spring_at(
    double elapsed, const std::optional<Spring>& blended_from) const noexcept
    -> std::optional<Spring> {
  const auto* found = std::get_if<Spring>(&curve_);
  if (found == nullptr || repetition_.has_value()) {
    return std::nullopt;
  }
  if (!blended_from.has_value()) {
    return *found;
  }
  return found->blended_toward(*blended_from, blend_duration_,
                               std::max(elapsed - delay_, 0.0) * speed_);
}

auto Animation::sample(const Span& span, double elapsed,
                       const std::optional<Spring>& blended_from) const noexcept
    -> Sample {
  AnimationContext context;
  return sample(span, elapsed, blended_from, context);
}

auto Animation::sample(const Span& span, double elapsed,
                       const std::optional<Spring>& blended_from,
                       AnimationContext& context) const noexcept -> Sample {
  const double started = elapsed - delay_;
  if (started < 0.0) {
    return Sample{.value = span.from, .velocity = {}, .finished = false};
  }
  const double time = started * speed_;
  if (const CustomAnimation* invention = custom_animation();
      invention != nullptr) {
    return invented(*invention, span, time, context);
  }
  if (const auto* found = std::get_if<Spring>(&curve_);
      found != nullptr && !repetition_.has_value()) {
    return physical(*found, span, time, blended_from);
  }
  const Sample done{.value = span.to, .velocity = {}, .finished = true};
  const double cycle = cycle_duration();
  if (cycle <= 0.0) {
    return done;
  }
  double local = time;
  bool reverse = false;
  if (!repetition_.has_value()) {
    if (time >= cycle) {
      return done;
    }
  } else {
    if (repetition_->count.has_value() &&
        time >= cycle * static_cast<double>(*repetition_->count)) {
      return done;
    }
    const double laps = std::floor(time / cycle);
    local = time - (laps * cycle);
    reverse = repetition_->autoreverses && std::fmod(laps, 2.0) >= 1.0;
    if (reverse) {
      local = cycle - local;
    }
  }
  const Progress progressed = progress(local, cycle);
  const AnimatableVector distance = span.to - span.from;
  const double rate = (reverse ? -progressed.rate : progressed.rate) * speed_;
  return Sample{.value = span.from + (distance * progressed.value),
                .velocity = distance * rate,
                .finished = false};
}

}
